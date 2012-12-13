/*****************************************************************************
 * bdagraph.cpp : DirectShow BDA graph for vlc
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * Copyright (C) 2011 Rémi Denis-Courmont
 * Copyright (C) 2012 John Freed
 *
 * Author: Ken Self <kenself(at)optusnet(dot)com(dot)au>
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
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_block.h>
#include "dtv/bdagraph.hpp"
#include "dtv/dtv.h"
#undef DEBUG_MONIKER_NAME

static ModulationType dvb_parse_modulation (const char *mod)
{
    if (!strcmp (mod, "16QAM"))   return BDA_MOD_16QAM;
    if (!strcmp (mod, "32QAM"))   return BDA_MOD_32QAM;
    if (!strcmp (mod, "64QAM"))   return BDA_MOD_64QAM;
    if (!strcmp (mod, "128QAM"))  return BDA_MOD_128QAM;
    if (!strcmp (mod, "256QAM"))  return BDA_MOD_256QAM;
    return BDA_MOD_NOT_SET;
}

static BinaryConvolutionCodeRate dvb_parse_fec (uint32_t fec)
{
    switch (fec)
    {
        case VLC_FEC(1,2): return BDA_BCC_RATE_1_2;
        case VLC_FEC(2,3): return BDA_BCC_RATE_2_3;
        case VLC_FEC(3,4): return BDA_BCC_RATE_3_4;
        case VLC_FEC(5,6): return BDA_BCC_RATE_5_6;
        case VLC_FEC(7,8): return BDA_BCC_RATE_7_8;
    }
    return BDA_BCC_RATE_NOT_SET;
}

static GuardInterval dvb_parse_guard (uint32_t guard)
{
    switch (guard)
    {
        case VLC_GUARD(1, 4): return BDA_GUARD_1_4;
        case VLC_GUARD(1, 8): return BDA_GUARD_1_8;
        case VLC_GUARD(1,16): return BDA_GUARD_1_16;
        case VLC_GUARD(1,32): return BDA_GUARD_1_32;
    }
    return BDA_GUARD_NOT_SET;
}

static TransmissionMode dvb_parse_transmission (int transmit)
{
    switch (transmit)
    {
        case 2: return BDA_XMIT_MODE_2K;
        case 8: return BDA_XMIT_MODE_8K;
    }
    return BDA_XMIT_MODE_NOT_SET;
}

static HierarchyAlpha dvb_parse_hierarchy (int hierarchy)
{
    switch (hierarchy)
    {
        case 1: return BDA_HALPHA_1;
        case 2: return BDA_HALPHA_2;
        case 4: return BDA_HALPHA_4;
    }
    return BDA_HALPHA_NOT_SET;
}

static Polarisation dvb_parse_polarization (char pol)
{
    switch (pol)
    {
        case 'H': return BDA_POLARISATION_LINEAR_H;
        case 'V': return BDA_POLARISATION_LINEAR_V;
        case 'L': return BDA_POLARISATION_CIRCULAR_L;
        case 'R': return BDA_POLARISATION_CIRCULAR_R;
    }
    return BDA_POLARISATION_NOT_SET;
}

static SpectralInversion dvb_parse_inversion (int inversion)
{
    switch (inversion)
    {
        case  0: return BDA_SPECTRAL_INVERSION_NORMAL;
        case  1: return BDA_SPECTRAL_INVERSION_INVERTED;
        case -1: return BDA_SPECTRAL_INVERSION_AUTOMATIC;
    }
    /* should never happen */
    return BDA_SPECTRAL_INVERSION_NOT_SET;
}

/****************************************************************************
 * Interfaces for calls from C
 ****************************************************************************/
struct dvb_device
{
    BDAGraph *module;

    /* DVB-S property cache */
    uint32_t frequency;
    uint32_t srate;
    uint32_t fec;
    char inversion;
    char pol;
    uint32_t lowf, highf, switchf;
};

dvb_device_t *dvb_open (vlc_object_t *obj)
{
    dvb_device_t *d = new dvb_device_t;

    d->module = new BDAGraph (obj);
    d->frequency = 0;
    d->srate = 0;
    d->fec = VLC_FEC_AUTO;
    d->inversion = -1;
    d->pol = 0;
    d->lowf = d->highf = d->switchf = 0;
    return d;
}

void dvb_close (dvb_device_t *d)
{
    delete d->module;
    delete d;
}

ssize_t dvb_read (dvb_device_t *d, void *buf, size_t len)
{
    return d->module->Pop(buf, len);
}

int dvb_add_pid (dvb_device_t *, uint16_t)
{
    return 0;
}

void dvb_remove_pid (dvb_device_t *, uint16_t)
{
}

unsigned dvb_enum_systems (dvb_device_t *d)
{
    return d->module->EnumSystems( );
}

float dvb_get_signal_strength (dvb_device_t *d)
{
    return d->module->GetSignalStrength( );
}

float dvb_get_snr (dvb_device_t *d)
{
    return d->module->GetSignalNoiseRatio( );
}

int dvb_set_inversion (dvb_device_t *d, int inversion)
{
    d->inversion = inversion;
    return d->module->SetInversion( d->inversion );
}

int dvb_tune (dvb_device_t *d)
{
    return d->module->SubmitTuneRequest ();
}

/* DVB-C */
int dvb_set_dvbc (dvb_device_t *d, uint32_t freq, const char *mod,
                  uint32_t srate, uint32_t /*fec*/)
{
    return d->module->SetDVBC (freq / 1000, mod, srate);
}

/* DVB-S */
int dvb_set_dvbs (dvb_device_t *d, uint64_t freq, uint32_t srate, uint32_t fec)
{
    d->frequency = freq / 1000;
    d->srate = srate;
    d->fec = fec;
    return d->module->SetDVBS(d->frequency, d->srate, d->fec, d->inversion,
                              d->pol, d->lowf, d->highf, d->switchf);
}

int dvb_set_dvbs2 (dvb_device_t *, uint64_t /*freq*/, const char * /*mod*/,
                   uint32_t /*srate*/, uint32_t /*fec*/, int /*pilot*/, int /*rolloff*/)
{
    return VLC_EGENERIC;
}

int dvb_set_sec (dvb_device_t *d, uint64_t freq, char pol,
                 uint32_t lowf, uint32_t highf, uint32_t switchf)
{
    d->frequency = freq / 1000;
    d->pol = pol;
    d->lowf = lowf;
    d->highf = highf;
    d->switchf = switchf;
    return d->module->SetDVBS(d->frequency, d->srate, d->fec, d->inversion,
                              d->pol, d->lowf, d->highf, d->switchf);
}

/* DVB-T */
int dvb_set_dvbt (dvb_device_t *d, uint32_t freq, const char * /*mod*/,
                  uint32_t fec_hp, uint32_t fec_lp, uint32_t bandwidth,
                  int transmission, uint32_t guard, int hierarchy)
{
    return d->module->SetDVBT(freq / 1000, fec_hp, fec_lp,
                              bandwidth, transmission, guard, hierarchy);
}

int dvb_set_dvbt2 (dvb_device_t *, uint32_t /*freq*/, const char * /*mod*/,
                   uint32_t /*fec*/, uint32_t /*bandwidth*/, int /*tx_mode*/,
                   uint32_t /*guard*/, uint32_t /*plp*/)
{
    return VLC_EGENERIC;
}

/* ISDB-C */
int dvb_set_isdbc (dvb_device_t *, uint32_t /*freq*/, const char * /*mod*/,
                   uint32_t /*srate*/, uint32_t /*fec*/)
{
    return VLC_EGENERIC;
}

/* ISDB-S */
int dvb_set_isdbs (dvb_device_t *, uint64_t /*freq*/, uint16_t /*ts_id*/)
{
    return VLC_EGENERIC;
}

/* ISDB-T */
int dvb_set_isdbt (dvb_device_t *, uint32_t /*freq*/, uint32_t /*bandwidth*/,
                   int /*transmit_mode*/, uint32_t /*guard*/,
                   const isdbt_layer_t /*layers*/[3])
{
    return VLC_EGENERIC;
}

/* ATSC */
int dvb_set_atsc (dvb_device_t *d, uint32_t freq, const char * /*mod*/)
{
    return d->module->SetATSC(freq / 1000);
}

int dvb_set_cqam (dvb_device_t *d, uint32_t freq, const char * /*mod*/)
{
    return d->module->SetCQAM(freq / 1000);
}

/*****************************************************************************
* BDAOutput
*****************************************************************************/
BDAOutput::BDAOutput( vlc_object_t *p_access ) :
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

ssize_t BDAOutput::Pop(void *buf, size_t len)
{
    vlc_mutex_locker l( &lock );

    mtime_t i_deadline = mdate() + 250 * 1000;
    while( !p_first )
    {
        if( vlc_cond_timedwait( &wait, &lock, i_deadline ) )
            return -1;
    }

    size_t i_index = 0;
    while( i_index < len )
    {
        size_t i_copy = __MIN( len - i_index, p_first->i_buffer );
        memcpy( (uint8_t *)buf + i_index, p_first->p_buffer, i_copy );

        i_index           += i_copy;

        p_first->p_buffer += i_copy;
        p_first->i_buffer -= i_copy;

        if( p_first->i_buffer <= 0 )
        {
            block_t *p_next = p_first->p_next;
            block_Release( p_first );

            p_first = p_next;
            if( !p_first )
            {
                pp_next = &p_first;
                break;
            }
        }
    }
    return i_index;
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
BDAGraph::BDAGraph( vlc_object_t *p_this ):
    p_access( p_this ),
    guid_network_type(GUID_NULL),
    l_tuner_used(-1),
    systems(0),
    d_graph_register( 0 ),
    output( p_this )
{
    p_media_control = NULL;

    p_tuning_space = NULL;

    p_filter_graph = NULL;
    p_system_dev_enum = NULL;
    p_network_provider = p_tuner_device = p_capture_device = NULL;
    p_sample_grabber = p_mpeg_demux = p_transport_info = NULL;
    p_scanning_tuner = NULL;
    p_grabber = NULL;

    CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );
}

/*****************************************************************************
* Destructor
*****************************************************************************/
BDAGraph::~BDAGraph()
{
    Destroy();

    if( p_tuning_space )
        p_tuning_space->Release();
    p_tuning_space = NULL;

    systems = 0;
    CoUninitialize();
}

/*****************************************************************************
* GetSystem
* helper function
*****************************************************************************/
unsigned BDAGraph::GetSystem( REFCLSID clsid )
{
    unsigned sys = 0;

    if( clsid == CLSID_DVBTNetworkProvider )
        sys = DVB_T;
    if( clsid == CLSID_DVBCNetworkProvider )
        sys = DVB_C;
    if( clsid == CLSID_DVBSNetworkProvider )
        sys = DVB_S;
    if( clsid == CLSID_ATSCNetworkProvider )
        sys = ATSC;
    if( clsid == CLSID_DigitalCableNetworkType )
        sys = CQAM;

    return sys;
}


/*****************************************************************************
 * Enumerate Systems
 *****************************************************************************
 * here is logic for special case where user uses an MRL that points
 * to DTV but is not fully specific. This is usually dvb:// and can come
 * either from a playlist, a channels.conf MythTV file, or from manual entry.
 *
 * Since this is done before the real tune request is submitted, we can
 * use the global device enumerator, etc., so long as we do a Destroy() at
 * the end
 *****************************************************************************/
unsigned BDAGraph::EnumSystems()
{
    HRESULT hr = S_OK;
    GUID guid_network_provider = GUID_NULL;

    msg_Dbg( p_access, "EnumSystems: Entering " );

    do
    {
        hr = GetNextNetworkType( &guid_network_provider );
        if( hr != S_OK ) break;
        hr = Check( guid_network_provider );
        if( FAILED( hr ) )
            msg_Dbg( p_access, "EnumSystems: Check failed, trying next" );
    }
    while( true );

    if( p_filter_graph )
        Destroy();
    msg_Dbg( p_access, "EnumSystems: Returning systems 0x%08x", systems );
    return systems;
}

float BDAGraph::GetSignalNoiseRatio(void)
{
    /* not implemented until Windows 7
     * IBDA_Encoder::GetState */
    return 0.;
}

float BDAGraph::GetSignalStrength(void)
{
    HRESULT hr = S_OK;
    long l_strength = 0;
    msg_Dbg( p_access, "GetSignalStrength: entering" );
    if( !p_scanning_tuner)
        return 0.;
    hr = p_scanning_tuner->get_SignalStrength( &l_strength );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "GetSignalStrength: "
            "Cannot get value: hr=0x%8lx", hr );
        return 0.;
    }
    msg_Dbg( p_access, "GetSignalStrength: got %ld", l_strength );
    if( l_strength == -1 )
        return -1.;
    return l_strength / 100.;
}

int BDAGraph::SubmitTuneRequest(void)
{
    HRESULT hr;
    int i = 0;

    /* Build and Start the Graph. If a Tuner device is in use the graph will
     * fail to start. Repeated calls to Build will check successive tuner
     * devices */
    do
    {
        msg_Dbg( p_access, "SubmitTuneRequest: Building the Graph" );

        hr = Build();
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "SubmitTuneRequest: "
                "Cannot Build the Graph: hr=0x%8lx", hr );
            return VLC_EGENERIC;
        }
        msg_Dbg( p_access, "SubmitTuneRequest: Starting the Graph" );

        hr = Start();
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "SubmitTuneRequest: "
                "Cannot Start the Graph, retrying: hr=0x%8lx", hr );
            ++i;
        }
    }
    while( hr != S_OK && i < 10 ); /* give up after 10 tries */

    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitTuneRequest: "
            "Failed to Start the Graph: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
* Set Clear QAM (DigitalCable)
* Local ATSC Digital Antenna
*****************************************************************************/
int BDAGraph::SetCQAM(long l_frequency)
{
    HRESULT hr = S_OK;
    class localComPtr
    {
        public:
        ITuneRequest*               p_tune_request;
        IDigitalCableTuneRequest*   p_cqam_tune_request;
        IDigitalCableLocator*       p_cqam_locator;
        localComPtr():
            p_tune_request(NULL),
            p_cqam_tune_request(NULL),
            p_cqam_locator(NULL)
            {};
        ~localComPtr()
        {
            if( p_tune_request )
                p_tune_request->Release();
            if( p_cqam_tune_request )
                p_cqam_tune_request->Release();
            if( p_cqam_locator )
                p_cqam_locator->Release();
        }
    } l;
    long l_minor_channel, l_physical_channel;

    l_physical_channel = var_GetInteger( p_access, "dvb-physical-channel" );
    l_minor_channel    = var_GetInteger( p_access, "dvb-minor-channel" );

    /* try to set p_scanning_tuner */
    hr = Check( CLSID_DigitalCableNetworkType );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetCQAM: "\
            "Cannot create Tuning Space: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    if( !p_scanning_tuner )
    {
        msg_Warn( p_access, "SetCQAM: Cannot get scanning tuner" );
        return VLC_EGENERIC;
    }

    hr = p_scanning_tuner->get_TuneRequest( &l.p_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetCQAM: "\
            "Cannot get Tune Request: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = l.p_tune_request->QueryInterface( IID_IDigitalCableTuneRequest,
        reinterpret_cast<void**>( &l.p_cqam_tune_request ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetCQAM: "\
                  "Cannot QI for IDigitalCableTuneRequest: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = ::CoCreateInstance( CLSID_DigitalCableLocator, 0, CLSCTX_INPROC,
        IID_IDigitalCableLocator, reinterpret_cast<void**>( &l.p_cqam_locator ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetCQAM: "\
                  "Cannot create the CQAM locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = S_OK;
    if( SUCCEEDED( hr ) && l_physical_channel > 0 )
        hr = l.p_cqam_locator->put_PhysicalChannel( l_physical_channel );
    if( SUCCEEDED( hr ) && l_frequency > 0 )
        hr = l.p_cqam_locator->put_CarrierFrequency( l_frequency );
    if( SUCCEEDED( hr ) && l_minor_channel > 0 )
        hr = l.p_cqam_tune_request->put_MinorChannel( l_minor_channel );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetCQAM: "\
                 "Cannot set tuning parameters: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = l.p_cqam_tune_request->put_Locator( l.p_cqam_locator );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetCQAM: "\
                  "Cannot put the locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_scanning_tuner->Validate( l.p_cqam_tune_request );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "SetCQAM: "\
            "Tune Request cannot be validated: hr=0x%8lx", hr );
    }
    /* increments ref count for scanning tuner */
    hr = p_scanning_tuner->put_TuneRequest( l.p_cqam_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetCQAM: "\
            "Cannot put the tune request: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
* Set ATSC
*****************************************************************************/
int BDAGraph::SetATSC(long l_frequency)
{
    HRESULT hr = S_OK;
    class localComPtr
    {
        public:
        ITuneRequest*               p_tune_request;
        IATSCChannelTuneRequest*    p_atsc_tune_request;
        IATSCLocator*               p_atsc_locator;
        localComPtr():
            p_tune_request(NULL),
            p_atsc_tune_request(NULL),
            p_atsc_locator(NULL)
            {};
        ~localComPtr()
        {
            if( p_tune_request )
                p_tune_request->Release();
            if( p_atsc_tune_request )
                p_atsc_tune_request->Release();
            if( p_atsc_locator )
                p_atsc_locator->Release();
        }
    } l;
    long l_major_channel, l_minor_channel, l_physical_channel;

    /* fixme: these parameters should have dtv prefix, not dvb */
    l_major_channel     = var_GetInteger( p_access, "dvb-major-channel" );
    l_minor_channel     = var_GetInteger( p_access, "dvb-minor-channel" );
    l_physical_channel  = var_GetInteger( p_access, "dvb-physical-channel" );

    /* try to set p_scanning_tuner */
    hr = Check( CLSID_ATSCNetworkProvider );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetATSC: "\
            "Cannot create Tuning Space: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    if( !p_scanning_tuner )
    {
        msg_Warn( p_access, "SetATSC: Cannot get scanning tuner" );
        return VLC_EGENERIC;
    }

    hr = p_scanning_tuner->get_TuneRequest( &l.p_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetATSC: "\
            "Cannot get Tune Request: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = l.p_tune_request->QueryInterface( IID_IATSCChannelTuneRequest,
        reinterpret_cast<void**>( &l.p_atsc_tune_request ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetATSC: "\
            "Cannot QI for IATSCChannelTuneRequest: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = ::CoCreateInstance( CLSID_ATSCLocator, 0, CLSCTX_INPROC,
        IID_IATSCLocator, reinterpret_cast<void**>( &l.p_atsc_locator ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetATSC: "\
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
        msg_Warn( p_access, "SetATSC: "\
            "Cannot set tuning parameters: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = l.p_atsc_tune_request->put_Locator( l.p_atsc_locator );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetATSC: "\
            "Cannot put the locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_scanning_tuner->Validate( l.p_atsc_tune_request );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "SetATSC: "\
            "Tune Request cannot be validated: hr=0x%8lx", hr );
    }
    /* increments ref count for scanning tuner */
    hr = p_scanning_tuner->put_TuneRequest( l.p_atsc_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetATSC: "\
            "Cannot put the tune request: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
* Set DVB-T
*
* This provides the tune request that everything else is built upon.
*
* Stores the tune request to the scanning tuner, where it is pulled out by
* dvb_tune a/k/a SubmitTuneRequest.
******************************************************************************/
int BDAGraph::SetDVBT(long l_frequency, uint32_t fec_hp, uint32_t fec_lp,
    long l_bandwidth, int transmission, uint32_t guard, int hierarchy)
{
    HRESULT hr = S_OK;

    class localComPtr
    {
        public:
        ITuneRequest*       p_tune_request;
        IDVBTuneRequest*    p_dvb_tune_request;
        IDVBTLocator*       p_dvbt_locator;
        IDVBTuningSpace2*   p_dvb_tuning_space;
        localComPtr():
            p_tune_request(NULL),
            p_dvb_tune_request(NULL),
            p_dvbt_locator(NULL),
            p_dvb_tuning_space(NULL)
            {};
        ~localComPtr()
        {
            if( p_tune_request )
                p_tune_request->Release();
            if( p_dvb_tune_request )
                p_dvb_tune_request->Release();
            if( p_dvbt_locator )
                p_dvbt_locator->Release();
            if( p_dvb_tuning_space )
                p_dvb_tuning_space->Release();
        }
    } l;

    /* create local dvbt-specific tune request and locator
     * then put it to existing scanning tuner */
    BinaryConvolutionCodeRate i_hp_fec = dvb_parse_fec(fec_hp);
    BinaryConvolutionCodeRate i_lp_fec = dvb_parse_fec(fec_lp);
    GuardInterval i_guard = dvb_parse_guard(guard);
    TransmissionMode i_transmission = dvb_parse_transmission(transmission);
    HierarchyAlpha i_hierarchy = dvb_parse_hierarchy(hierarchy);

    /* try to set p_scanning_tuner */
    msg_Dbg( p_access, "SetDVBT: set up scanning tuner" );
    hr = Check( CLSID_DVBTNetworkProvider );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBT: "\
            "Cannot create Tuning Space: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    if( !p_scanning_tuner )
    {
        msg_Warn( p_access, "SetDVBT: Cannot get scanning tuner" );
        return VLC_EGENERIC;
    }

    hr = p_scanning_tuner->get_TuneRequest( &l.p_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBT: "\
            "Cannot get Tune Request: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBT: Creating DVB tune request" );
    hr = l.p_tune_request->QueryInterface( IID_IDVBTuneRequest,
        reinterpret_cast<void**>( &l.p_dvb_tune_request ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBT: "\
            "Cannot QI for IDVBTuneRequest: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    l.p_dvb_tune_request->put_ONID( -1 );
    l.p_dvb_tune_request->put_SID( -1 );
    l.p_dvb_tune_request->put_TSID( -1 );

    msg_Dbg( p_access, "SetDVBT: get TS" );
    hr = p_scanning_tuner->get_TuningSpace( &p_tuning_space );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "SetDVBT: "\
            "cannot get tuning space: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBT: QI to DVBT TS" );
    hr = p_tuning_space->QueryInterface( IID_IDVBTuningSpace2,
        reinterpret_cast<void**>( &l.p_dvb_tuning_space ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBT: "\
            "Cannot QI for IDVBTuningSpace2: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBT: Creating local locator" );
    hr = ::CoCreateInstance( CLSID_DVBTLocator, 0, CLSCTX_INPROC,
        IID_IDVBTLocator, reinterpret_cast<void**>( &l.p_dvbt_locator ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBT: "\
            "Cannot create the DVBT Locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

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
        msg_Warn( p_access, "SetDVBT: "\
            "Cannot set tuning parameters on Locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBT: putting DVBT locator into local tune request" );

    hr = l.p_dvb_tune_request->put_Locator( l.p_dvbt_locator );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBT: "\
            "Cannot put the locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBT: putting local Tune Request to scanning tuner" );
    hr = p_scanning_tuner->Validate( l.p_dvb_tune_request );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "SetDVBT: "\
            "Tune Request cannot be validated: hr=0x%8lx", hr );
    }
    /* increments ref count for scanning tuner */
    hr = p_scanning_tuner->put_TuneRequest( l.p_dvb_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBT: "\
            "Cannot put the tune request: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBT: return success" );
    return VLC_SUCCESS;
}

/*****************************************************************************
* Set DVB-C
******************************************************************************/
int BDAGraph::SetDVBC(long l_frequency, const char *mod, long l_symbolrate)
{
    HRESULT hr = S_OK;

    class localComPtr
    {
        public:
        ITuneRequest*       p_tune_request;
        IDVBTuneRequest*    p_dvb_tune_request;
        IDVBCLocator*       p_dvbc_locator;
        IDVBTuningSpace2*   p_dvb_tuning_space;

        localComPtr():
            p_tune_request(NULL),
            p_dvb_tune_request(NULL),
            p_dvbc_locator(NULL),
            p_dvb_tuning_space(NULL)
            {};
        ~localComPtr()
        {
            if( p_tune_request )
                p_tune_request->Release();
            if( p_dvb_tune_request )
                p_dvb_tune_request->Release();
            if( p_dvbc_locator )
                p_dvbc_locator->Release();
            if( p_dvb_tuning_space )
                p_dvb_tuning_space->Release();
        }
    } l;

    ModulationType i_qam_mod = dvb_parse_modulation(mod);

    /* try to set p_scanning_tuner */
    hr = Check( CLSID_DVBCNetworkProvider );
    msg_Dbg( p_access, "SetDVBC: returned from Check" );

    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBC: "\
            "Cannot create Tuning Space: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBC: check on scanning tuner" );
    if( !p_scanning_tuner )
    {
        msg_Warn( p_access, "SetDVBC: Cannot get scanning tuner" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBC: get tune request" );
    hr = p_scanning_tuner->get_TuneRequest( &l.p_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBC: "\
            "Cannot get Tune Request: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBC: QI for dvb tune request" );
    hr = l.p_tune_request->QueryInterface( IID_IDVBTuneRequest,
        reinterpret_cast<void**>( &l.p_dvb_tune_request ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBC: "\
            "Cannot QI for IDVBTuneRequest: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    l.p_dvb_tune_request->put_ONID( -1 );
    l.p_dvb_tune_request->put_SID( -1 );
    l.p_dvb_tune_request->put_TSID( -1 );

    msg_Dbg( p_access, "SetDVBC: create dvbc locator" );
    hr = ::CoCreateInstance( CLSID_DVBCLocator, 0, CLSCTX_INPROC,
        IID_IDVBCLocator, reinterpret_cast<void**>( &l.p_dvbc_locator ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBC: "\
            "Cannot create the DVB-C Locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }


    msg_Dbg( p_access, "SetDVBC: get TS" );
    hr = p_scanning_tuner->get_TuningSpace( &p_tuning_space );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "SetDVBC: "\
            "cannot get tuning space: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBC: QI for dvb tuning space" );
    hr = p_tuning_space->QueryInterface( IID_IDVBTuningSpace2,
        reinterpret_cast<void**>( &l.p_dvb_tuning_space ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBC: "\
            "Cannot QI for IDVBTuningSpace2: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBC: set up locator" );
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
        msg_Warn( p_access, "SetDVBC: "\
            "Cannot set tuning parameters on Locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBC: put locator to dvb tune request" );
    hr = l.p_dvb_tune_request->put_Locator( l.p_dvbc_locator );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBC: "\
            "Cannot put the locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBC: validate dvb tune request" );
    hr = p_scanning_tuner->Validate( l.p_dvb_tune_request );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "SetDVBC: "\
            "Tune Request cannot be validated: hr=0x%8lx", hr );
    }

    /* increments ref count for scanning tuner */
    msg_Dbg( p_access, "SetDVBC: put dvb tune request to tuner" );
    hr = p_scanning_tuner->put_TuneRequest( l.p_dvb_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBC: "\
            "Cannot put the tune request: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_access, "SetDVBC: return success" );

    return VLC_SUCCESS;
}

/*****************************************************************************
* Set Inversion
******************************************************************************/
int BDAGraph::SetInversion(int inversion)
{
    HRESULT hr = S_OK;
    class localComPtr
    {
        public:
        IDVBSTuningSpace*   p_dvbs_tuning_space;
        localComPtr() :
            p_dvbs_tuning_space(NULL)
        {}
        ~localComPtr()
        {
            if( p_dvbs_tuning_space )
                p_dvbs_tuning_space->Release();
        }
    } l;

    SpectralInversion i_inversion = dvb_parse_inversion( inversion );

    if( !p_scanning_tuner )
    {
        msg_Warn( p_access, "SetInversion: "\
            "No scanning tuner" );
        return VLC_EGENERIC;
    }

    /* SetInversion is called for all DVB tuners, before the dvb_tune(),
     * in access.c. Since DVBT and DVBC don't support spectral
     * inversion, we need to return VLC_SUCCESS in those cases
     * so that dvb_tune() will be called */
    if( ( GetSystem( guid_network_type ) & ( DVB_S | DVB_S2 | ISDB_S ) ) == 0 )
    {
        msg_Dbg( p_access, "SetInversion: Not Satellite type" );
        return VLC_SUCCESS;
    }

    msg_Dbg( p_access, "SetInversion: get TS" );
    hr = p_scanning_tuner->get_TuningSpace( &p_tuning_space );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetInversion: "\
            "cannot get tuning space: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_tuning_space->QueryInterface( IID_IDVBSTuningSpace,
        reinterpret_cast<void**>( &l.p_dvbs_tuning_space ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetInversion: "\
            "Cannot QI for IDVBSTuningSpace: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    if( i_inversion != BDA_SPECTRAL_INVERSION_NOT_SET )
        hr = l.p_dvbs_tuning_space->put_SpectralInversion( i_inversion );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetInversion: "\
            "Cannot put inversion: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
* Set DVB-S
******************************************************************************/
int BDAGraph::SetDVBS(long l_frequency, long l_symbolrate, uint32_t fec,
                      int inversion, char pol,
                      long l_lnb_lof1, long l_lnb_lof2, long l_lnb_slof)
{
    HRESULT hr = S_OK;

    class localComPtr
    {
        public:
        ITuneRequest*       p_tune_request;
        IDVBTuneRequest*    p_dvb_tune_request;
        IDVBSLocator*       p_dvbs_locator;
        IDVBSTuningSpace*   p_dvbs_tuning_space;
        char*               psz_polarisation;
        char*               psz_input_range;
        BSTR                bstr_input_range;
        WCHAR*              pwsz_input_range;
        int                 i_range_len;
        localComPtr() :
            p_tune_request(NULL),
            p_dvb_tune_request(NULL),
            p_dvbs_locator(NULL),
            p_dvbs_tuning_space(NULL),
            psz_polarisation(NULL),
            psz_input_range(NULL),
            bstr_input_range(NULL),
            pwsz_input_range(NULL),
            i_range_len(0)
        {}
        ~localComPtr()
        {
            if( p_tune_request )
                p_tune_request->Release();
            if( p_dvb_tune_request )
                p_dvb_tune_request->Release();
            if( p_dvbs_locator )
                p_dvbs_locator->Release();
            if( p_dvbs_tuning_space )
                p_dvbs_tuning_space->Release();
            SysFreeString( bstr_input_range );
            if( pwsz_input_range )
                delete[] pwsz_input_range;
            free( psz_input_range );
            free( psz_polarisation );
        }
    } l;
    long l_azimuth, l_elevation, l_longitude;
    long l_network_id;
    VARIANT_BOOL b_west;

    BinaryConvolutionCodeRate i_hp_fec = dvb_parse_fec( fec );
    Polarisation i_polar = dvb_parse_polarization( pol );
    SpectralInversion i_inversion = dvb_parse_inversion( inversion );

    l_azimuth          = var_GetInteger( p_access, "dvb-azimuth" );
    l_elevation        = var_GetInteger( p_access, "dvb-elevation" );
    l_longitude        = var_GetInteger( p_access, "dvb-longitude" );
    l_network_id       = var_GetInteger( p_access, "dvb-network-id" );

    if( asprintf( &l.psz_polarisation, "%c", pol ) == -1 )
        abort();

    b_west = ( l_longitude < 0 );

    l.psz_input_range  = var_GetNonEmptyString( p_access, "dvb-range" );
    l.i_range_len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED,
        l.psz_input_range, -1, l.pwsz_input_range, 0 );
    if( l.i_range_len > 0 )
    {
        l.pwsz_input_range = new WCHAR[l.i_range_len];
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED,
            l.psz_input_range, -1, l.pwsz_input_range, l.i_range_len );
        l.bstr_input_range = SysAllocString( l.pwsz_input_range );
    }

    /* try to set p_scanning_tuner */
    hr = Check( CLSID_DVBSNetworkProvider );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBS: "\
            "Cannot create Tuning Space: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    if( !p_scanning_tuner )
    {
        msg_Warn( p_access, "SetDVBS: Cannot get scanning tuner" );
        return VLC_EGENERIC;
    }

    hr = p_scanning_tuner->get_TuneRequest( &l.p_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBS: "\
            "Cannot get Tune Request: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = l.p_tune_request->QueryInterface( IID_IDVBTuneRequest,
        reinterpret_cast<void**>( &l.p_dvb_tune_request ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBS: "\
            "Cannot QI for IDVBTuneRequest: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    l.p_dvb_tune_request->put_ONID( -1 );
    l.p_dvb_tune_request->put_SID( -1 );
    l.p_dvb_tune_request->put_TSID( -1 );

    hr = ::CoCreateInstance( CLSID_DVBSLocator, 0, CLSCTX_INPROC,
        IID_IDVBSLocator, reinterpret_cast<void**>( &l.p_dvbs_locator ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBS: "\
            "Cannot create the DVBS Locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "SetDVBS: get TS" );
    hr = p_scanning_tuner->get_TuningSpace( &p_tuning_space );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "SetDVBS: "\
            "cannot get tuning space: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_tuning_space->QueryInterface( IID_IDVBSTuningSpace,
        reinterpret_cast<void**>( &l.p_dvbs_tuning_space ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBS: "\
            "Cannot QI for IDVBSTuningSpace: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

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
    if( SUCCEEDED( hr ) )
        hr = l.p_dvbs_locator->put_Modulation( BDA_MOD_QPSK );
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
        msg_Warn( p_access, "SetDVBS: "\
            "Cannot set tuning parameters on Locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = l.p_dvb_tune_request->put_Locator( l.p_dvbs_locator );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBS: "\
            "Cannot put the locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_scanning_tuner->Validate( l.p_dvb_tune_request );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "SetDVBS: "\
            "Tune Request cannot be validated: hr=0x%8lx", hr );
    }

    /* increments ref count for scanning tuner */
    hr = p_scanning_tuner->put_TuneRequest( l.p_dvb_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetDVBS: "\
            "Cannot put the tune request: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
* SetUpTuner
******************************************************************************
* Sets up global p_scanning_tuner and sets guid_network_type according
* to the Network Type requested.
*
* Logic: if tuner is set up and is the right network type, use it.
* Otherwise, poll the tuner for the right tuning space. 
*
* Then set up a tune request and try to validate it. Finally, put
* tune request and tuning space to tuner
*
* on success, sets globals: p_scanning_tuner and guid_network_type
*
******************************************************************************/
HRESULT BDAGraph::SetUpTuner( REFCLSID guid_this_network_type )
{
    HRESULT hr = S_OK;
    class localComPtr
    {
        public:
        ITuningSpaceContainer*      p_tuning_space_container;
        IEnumTuningSpaces*          p_tuning_space_enum;
        ITuningSpace*               p_test_tuning_space;
        ITuneRequest*               p_tune_request;
        IDVBTuneRequest*            p_dvb_tune_request;

        IDigitalCableTuneRequest*   p_cqam_tune_request;
        IATSCChannelTuneRequest*    p_atsc_tune_request;
        ILocator*                   p_locator;
        IDVBTLocator*               p_dvbt_locator;
        IDVBCLocator*               p_dvbc_locator;
        IDVBSLocator*               p_dvbs_locator;

        BSTR                        bstr_name;

        CLSID                       guid_test_network_type;
        char*                       psz_network_name;
        char*                       psz_bstr_name;
        int                         i_name_len;

        localComPtr():
            p_tuning_space_container(NULL),
            p_tuning_space_enum(NULL),
            p_test_tuning_space(NULL),
            p_tune_request(NULL),
            p_dvb_tune_request(NULL),
            p_cqam_tune_request(NULL),
            p_atsc_tune_request(NULL),
            p_locator(NULL),
            p_dvbt_locator(NULL),
            p_dvbc_locator(NULL),
            p_dvbs_locator(NULL),
            bstr_name(NULL),
            guid_test_network_type(GUID_NULL),
            psz_network_name(NULL),
            psz_bstr_name(NULL),
            i_name_len(0)
        {}
        ~localComPtr()
        {
            if( p_tuning_space_enum )
                p_tuning_space_enum->Release();
            if( p_tuning_space_container )
                p_tuning_space_container->Release();
            if( p_test_tuning_space )
                p_test_tuning_space->Release();
            if( p_tune_request )
                p_tune_request->Release();
            if( p_dvb_tune_request )
                p_dvb_tune_request->Release();
            if( p_cqam_tune_request )
                p_cqam_tune_request->Release();
            if( p_atsc_tune_request )
                p_atsc_tune_request->Release();
            if( p_locator )
                p_locator->Release();
            if( p_dvbt_locator )
                p_dvbt_locator->Release();
            if( p_dvbc_locator )
                p_dvbc_locator->Release();
            if( p_dvbs_locator )
                p_dvbs_locator->Release();
            SysFreeString( bstr_name );
            delete[] psz_bstr_name;
            free( psz_network_name );
        }
    } l;

    msg_Dbg( p_access, "SetUpTuner: entering" );


    /* We shall test for a specific Tuning space name supplied on the command
     * line as dvb-network-name=xxx.
     * For some users with multiple cards and/or multiple networks this could
     * be useful. This allows us to reasonably safely apply updates to the
     * System Tuning Space in the registry without disrupting other streams. */

    l.psz_network_name = var_GetNonEmptyString( p_access, "dvb-network-name" );

    if( l.psz_network_name )
    {
        msg_Dbg( p_access, "SetUpTuner: Find Tuning Space: %s",
            l.psz_network_name );
    }
    else
    {
        l.psz_network_name = new char[1];
        *l.psz_network_name = '\0';
    }

    /* Tuner may already have been set up. If it is for the same
     * network type then all is well. Otherwise, reset the Tuning Space and get
     * a new one */
    msg_Dbg( p_access, "SetUpTuner: Checking for tuning space" );
    if( !p_scanning_tuner )
    {
        msg_Warn( p_access, "SetUpTuner: "\
            "Cannot find scanning tuner" );
        return E_FAIL;
    }

    if( p_tuning_space )
    {
        msg_Dbg( p_access, "SetUpTuner: get network type" );
        hr = p_tuning_space->get__NetworkType( &l.guid_test_network_type );
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "Check: "\
                "Cannot get network type: hr=0x%8lx", hr );
            l.guid_test_network_type = GUID_NULL;
        }

        msg_Dbg( p_access, "SetUpTuner: see if it's the right one" );
        if( l.guid_test_network_type == guid_this_network_type )
        {
            msg_Dbg( p_access, "SetUpTuner: it's the right one" );
            SysFreeString( l.bstr_name );

            hr = p_tuning_space->get_UniqueName( &l.bstr_name );
            if( FAILED( hr ) )
            {
                /* should never fail on a good tuning space */
                msg_Dbg( p_access, "SetUpTuner: "\
                    "Cannot get UniqueName for Tuning Space: hr=0x%8lx", hr );
                goto NoTuningSpace;
            }

            /* Test for a specific Tuning space name supplied on the command
             * line as dvb-network-name=xxx */
            if( l.psz_bstr_name )
                delete[] l.psz_bstr_name;
            l.i_name_len = WideCharToMultiByte( CP_ACP, 0, l.bstr_name, -1,
                l.psz_bstr_name, 0, NULL, NULL );
            l.psz_bstr_name = new char[ l.i_name_len ];
            l.i_name_len = WideCharToMultiByte( CP_ACP, 0, l.bstr_name, -1,
                l.psz_bstr_name, l.i_name_len, NULL, NULL );

            /* if no name was requested on command line, or if the name
             * requested equals the name of this space, we are OK */
            if( *l.psz_network_name == '\0' ||
                strcmp( l.psz_network_name, l.psz_bstr_name ) == 0 )
            {
                msg_Dbg( p_access, "SetUpTuner: Using Tuning Space: %s",
                    l.psz_bstr_name );
                /* p_tuning_space and guid_network_type are already set */
                /* you probably already have a tune request, also */
                hr = p_scanning_tuner->get_TuneRequest( &l.p_tune_request );
                if( SUCCEEDED( hr ) )
                {
                    return S_OK;
                }
                /* CreateTuneRequest adds l.p_tune_request to p_tuning_space
                 * l.p_tune_request->RefCount = 1 */
                hr = p_tuning_space->CreateTuneRequest( &l.p_tune_request );
                if( SUCCEEDED( hr ) )
                {
                    return S_OK;
                }
                msg_Warn( p_access, "SetUpTuner: "\
                    "Cannot Create Tune Request: hr=0x%8lx", hr );
               /* fall through to NoTuningSpace */
            }
        }

        /* else different guid_network_type */
    NoTuningSpace:
        if( p_tuning_space )
            p_tuning_space->Release();
        p_tuning_space = NULL;
        /* pro forma; should have returned S_OK if we created this */
        if( l.p_tune_request )
            l.p_tune_request->Release();
        l.p_tune_request = NULL;
    }


    /* p_tuning_space is null at this point; we have already
       returned S_OK if it was good. So find a tuning
       space on the scanning tuner. */

    msg_Dbg( p_access, "SetUpTuner: release TuningSpaces Enumerator" );
    if( l.p_tuning_space_enum )
        l.p_tuning_space_enum->Release();
    msg_Dbg( p_access, "SetUpTuner: nullify TuningSpaces Enumerator" );
    l.p_tuning_space_enum = NULL;
    msg_Dbg( p_access, "SetUpTuner: create TuningSpaces Enumerator" );

    hr = p_scanning_tuner->EnumTuningSpaces( &l.p_tuning_space_enum );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetUpTuner: "\
            "Cannot create TuningSpaces Enumerator: hr=0x%8lx", hr );
        goto TryToClone;
    }

    do
    {
        msg_Dbg( p_access, "SetUpTuner: top of loop" );
        l.guid_test_network_type = GUID_NULL;
        if( l.p_test_tuning_space )
            l.p_test_tuning_space->Release();
        l.p_test_tuning_space = NULL;
        if( p_tuning_space )
            p_tuning_space->Release();
        p_tuning_space = NULL;
        SysFreeString( l.bstr_name );
        msg_Dbg( p_access, "SetUpTuner: need good TS enum" );
        if( !l.p_tuning_space_enum ) break;
        msg_Dbg( p_access, "SetUpTuner: next tuning space" );
        hr = l.p_tuning_space_enum->Next( 1, &l.p_test_tuning_space, NULL );
        if( hr != S_OK ) break;
        msg_Dbg( p_access, "SetUpTuner: get network type" );
        hr = l.p_test_tuning_space->get__NetworkType( &l.guid_test_network_type );
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "Check: "\
                "Cannot get network type: hr=0x%8lx", hr );
            l.guid_test_network_type = GUID_NULL;
        }
        if( l.guid_test_network_type == guid_this_network_type )
        {
            msg_Dbg( p_access, "SetUpTuner: Found matching space on tuner" );

            SysFreeString( l.bstr_name );
            msg_Dbg( p_access, "SetUpTuner: get unique name" );

            hr = l.p_test_tuning_space->get_UniqueName( &l.bstr_name );
            if( FAILED( hr ) )
            {
                /* should never fail on a good tuning space */
                msg_Dbg( p_access, "SetUpTuner: "\
                    "Cannot get UniqueName for Tuning Space: hr=0x%8lx", hr );
                continue;
            }
            msg_Dbg( p_access, "SetUpTuner: convert w to multi" );
            if ( l.psz_bstr_name )
                delete[] l.psz_bstr_name;
            l.i_name_len = WideCharToMultiByte( CP_ACP, 0, l.bstr_name, -1,
                l.psz_bstr_name, 0, NULL, NULL );
            l.psz_bstr_name = new char[ l.i_name_len ];
            l.i_name_len = WideCharToMultiByte( CP_ACP, 0, l.bstr_name, -1,
                l.psz_bstr_name, l.i_name_len, NULL, NULL );
            msg_Dbg( p_access, "SetUpTuner: Using Tuning Space: %s",
                l.psz_bstr_name );
            break;
        }

    }
    while( true );
    msg_Dbg( p_access, "SetUpTuner: checking what we got" );

    if( l.guid_test_network_type == GUID_NULL)
    {
        msg_Dbg( p_access, "SetUpTuner: got null, try to clone" );
        goto TryToClone;
    }

    msg_Dbg( p_access, "SetUpTuner: put TS" );
    hr = p_scanning_tuner->put_TuningSpace( l.p_test_tuning_space );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "SetUpTuner: "\
            "cannot put tuning space: hr=0x%8lx", hr );
        goto TryToClone;
    }

    msg_Dbg( p_access, "SetUpTuner: get default locator" );
    hr = l.p_test_tuning_space->get_DefaultLocator( &l.p_locator );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "SetUpTuner: "\
            "cannot get default locator: hr=0x%8lx", hr );
        goto TryToClone;
    }

    msg_Dbg( p_access, "SetUpTuner: create tune request" );
    hr = l.p_test_tuning_space->CreateTuneRequest( &l.p_tune_request );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "SetUpTuner: "\
            "cannot create tune request: hr=0x%8lx", hr );
        goto TryToClone;
    }

    msg_Dbg( p_access, "SetUpTuner: put locator" );
    hr = l.p_tune_request->put_Locator( l.p_locator );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "SetUpTuner: "\
            "cannot put locator: hr=0x%8lx", hr );
        goto TryToClone;
    }

    msg_Dbg( p_access, "SetUpTuner: try to validate tune request" );
    hr = p_scanning_tuner->Validate( l.p_tune_request );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "SetUpTuner: "\
            "Tune Request cannot be validated: hr=0x%8lx", hr );
    }

    msg_Dbg( p_access, "SetUpTuner: Attach tune request to Scanning Tuner");
    /* increments ref count for scanning tuner */
    hr = p_scanning_tuner->put_TuneRequest( l.p_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SetUpTuner: "\
            "Cannot submit the tune request: hr=0x%8lx", hr );
        return hr;
    }

    msg_Dbg( p_access, "SetUpTuner: Tuning Space and Tune Request created" );
    return S_OK;

    /* Get the SystemTuningSpaces container
     * p_tuning_space_container->Refcount = 1  */
TryToClone:
    msg_Warn( p_access, "SetUpTuner: won't try to clone " );
    return E_FAIL;
}

/*****************************************************************************
* GetNextNetworkType
* helper function; this is probably best done as an Enumeration of
* network providers
*****************************************************************************/
HRESULT BDAGraph::GetNextNetworkType( CLSID* guid_this_network_type )
{
    HRESULT hr = S_OK;
    if( *guid_this_network_type == GUID_NULL )
    {
        msg_Dbg( p_access, "GetNextNetworkType: DVB-C" );
        *guid_this_network_type = CLSID_DVBCNetworkProvider;
        return S_OK;
    }
    if( *guid_this_network_type == CLSID_DVBCNetworkProvider )
    {
        msg_Dbg( p_access, "GetNextNetworkType: DVB-T" );
        *guid_this_network_type = CLSID_DVBTNetworkProvider;
        return S_OK;
    }
    if( *guid_this_network_type == CLSID_DVBTNetworkProvider )
    {
        msg_Dbg( p_access, "GetNextNetworkType: DVB-S" );
        *guid_this_network_type = CLSID_DVBSNetworkProvider;
        return S_OK;
    }
    if( *guid_this_network_type == CLSID_DVBSNetworkProvider )
    {
        msg_Dbg( p_access, "GetNextNetworkType: ATSC" );
        *guid_this_network_type = CLSID_ATSCNetworkProvider;
        return S_OK;
    }
    msg_Dbg( p_access, "GetNextNetworkType: failed" );
    *guid_this_network_type = GUID_NULL;
    hr = E_FAIL;
    return hr;
}


/******************************************************************************
* Check
*******************************************************************************
* Check if tuner supports this network type
*
* on success, sets globals:
* systems, l_tuner_used, p_network_provider, p_scanning_tuner, p_tuner_device,
* p_tuning_space, p_filter_graph
******************************************************************************/
HRESULT BDAGraph::Check( REFCLSID guid_this_network_type )
{
    HRESULT hr = S_OK;

    class localComPtr
    {
        public:
        ITuningSpaceContainer*  p_tuning_space_container;

        localComPtr():
             p_tuning_space_container(NULL)
        {};
        ~localComPtr()
        {
            if( p_tuning_space_container )
                p_tuning_space_container->Release();
        }
    } l;

    msg_Dbg( p_access, "Check: entering ");

    /* Note that the systems global is persistent across Destroy().
     * So we need to see if a tuner has been physically removed from
     * the system since the last Check. Before we do anything,
     * assume that this Check will fail and remove this network type
     * from systems. It will be restored if the Check passes.
     */

    systems &= ~( GetSystem( guid_this_network_type ) );


    /* If we have already have a filter graph, rebuild it*/
    msg_Dbg( p_access, "Check: Destroying filter graph" );
    if( p_filter_graph )
        Destroy();
    p_filter_graph = NULL;
    hr = ::CoCreateInstance( CLSID_FilterGraph, NULL, CLSCTX_INPROC,
        IID_IGraphBuilder, reinterpret_cast<void**>( &p_filter_graph ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Check: "\
            "Cannot CoCreate IFilterGraph: hr=0x%8lx", hr );
        return hr;
    }

    /* First filter in the graph is the Network Provider and
     * its Scanning Tuner which takes the Tune Request */
    if( p_network_provider )
        p_network_provider->Release();
    p_network_provider = NULL;
    hr = ::CoCreateInstance( guid_this_network_type, NULL, CLSCTX_INPROC_SERVER,
        IID_IBaseFilter, reinterpret_cast<void**>( &p_network_provider ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Check: "\
            "Cannot CoCreate Network Provider: hr=0x%8lx", hr );
        return hr;
    }

    msg_Dbg( p_access, "Check: adding Network Provider to graph");
    hr = p_filter_graph->AddFilter( p_network_provider, L"Network Provider" );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Check: "\
            "Cannot load network provider: hr=0x%8lx", hr );
        return hr;
    }

    /* Add the Network Tuner to the Network Provider. On subsequent calls,
     * l_tuner_used will cause a different tuner to be selected.
     *
     * To select a specific device first get the parameter that nominates the
     * device (dvb-adapter) and use the value to initialise l_tuner_used.
     * Note that dvb-adapter is 1-based, while l_tuner_used is 0-based.
     * When FindFilter returns, check the contents of l_tuner_used.
     * If it is not what was selected, then the requested device was not
     * available, so return with an error. */

    long l_adapter = -1;
    l_adapter = var_GetInteger( p_access, "dvb-adapter" );
    if( l_tuner_used < 0 && l_adapter >= 0 )
        l_tuner_used = l_adapter - 1;

    /* If tuner is in cold state, we have to do a successful put_TuneRequest
     * before it will Connect. */
    msg_Dbg( p_access, "Check: Creating Scanning Tuner");
    if( p_scanning_tuner )
        p_scanning_tuner->Release();
    p_scanning_tuner = NULL;
    hr = p_network_provider->QueryInterface( IID_IScanningTuner,
        reinterpret_cast<void**>( &p_scanning_tuner ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Check: "\
            "Cannot QI Network Provider for Scanning Tuner: hr=0x%8lx", hr );
        return hr;
    }

    /* try to set up p_scanning_tuner */
    msg_Dbg( p_access, "Check: Calling SetUpTuner" );
    hr = SetUpTuner( guid_this_network_type );
    if( FAILED( hr ) )
    {
        msg_Dbg( p_access, "Check: "\
            "Cannot set up scanner in Check mode: hr=0x%8lx", hr );
        return hr;
    }

    msg_Dbg( p_access, "Check: "\
        "Calling FindFilter for KSCATEGORY_BDA_NETWORK_TUNER with "\
        "p_network_provider; l_tuner_used=%ld", l_tuner_used );
    hr = FindFilter( KSCATEGORY_BDA_NETWORK_TUNER, &l_tuner_used,
        p_network_provider, &p_tuner_device );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Check: "\
            "Cannot load tuner device and connect network provider: "\
            "hr=0x%8lx", hr );
        return hr;
    }

    if( l_adapter > 0 && l_tuner_used != l_adapter )
    {
         msg_Warn( p_access, "Check: "\
             "Tuner device %ld is not available", l_adapter );
         return E_FAIL;
    }

    msg_Dbg( p_access, "Check: Using adapter %ld", l_tuner_used );
    /* success!
     * already set l_tuner_used,
     * p_tuning_space
     */
    msg_Dbg( p_access, "Check: check succeeded: hr=0x%8lx", hr );
    systems |= GetSystem( guid_this_network_type );
    msg_Dbg( p_access, "Check: returning from Check mode" );
    return S_OK;
}


/******************************************************************************
* Build
*******************************************************************************
* Build the Filter Graph
*
* connects filters and
* creates the media control and registers the graph
* on success, sets globals:
* d_graph_register, p_media_control, p_grabber, p_sample_grabber,
* p_mpeg_demux, p_transport_info
******************************************************************************/
HRESULT BDAGraph::Build()
{
    HRESULT hr = S_OK;
    long            l_capture_used;
    long            l_tif_used;
    AM_MEDIA_TYPE   grabber_media_type;

    class localComPtr
    {
        public:
        ITuningSpaceContainer*  p_tuning_space_container;
        localComPtr():
            p_tuning_space_container(NULL)
        {};
        ~localComPtr()
        {
            if( p_tuning_space_container )
                p_tuning_space_container->Release();
        }
    } l;

    msg_Dbg( p_access, "Build: entering");

    /* at this point, you've connected to a scanning tuner of the right
     * network type.
     */
    if( !p_scanning_tuner || !p_tuner_device )
    {
        msg_Warn( p_access, "Build: "\
            "Scanning Tuner does not exist" );
        return E_FAIL;
    }

    hr = p_scanning_tuner->get_TuneRequest( &p_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: no tune request" );
        return hr;
    }
    hr = p_scanning_tuner->get_TuningSpace( &p_tuning_space );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: no tuning space" );
        return hr;
    }
    hr = p_tuning_space->get__NetworkType( &guid_network_type );


    /* Always look for all capture devices to match the Network Tuner */
    l_capture_used = -1;
    msg_Dbg( p_access, "Build: "\
        "Calling FindFilter for KSCATEGORY_BDA_RECEIVER_COMPONENT with "\
        "p_tuner_device; l_capture_used=%ld", l_capture_used );
    hr = FindFilter( KSCATEGORY_BDA_RECEIVER_COMPONENT, &l_capture_used,
        p_tuner_device, &p_capture_device );
    if( FAILED( hr ) )
    {
        /* Some BDA drivers do not provide a Capture Device Filter so force
         * the Sample Grabber to connect directly to the Tuner Device */
        msg_Dbg( p_access, "Build: "\
            "Cannot find Capture device. Connect to tuner "\
            "and AddRef() : hr=0x%8lx", hr );
        p_capture_device = p_tuner_device;
        p_capture_device->AddRef();
    }

    if( p_sample_grabber )
         p_sample_grabber->Release();
    p_sample_grabber = NULL;
    /* Insert the Sample Grabber to tap into the Transport Stream. */
    hr = ::CoCreateInstance( CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
        IID_IBaseFilter, reinterpret_cast<void**>( &p_sample_grabber ) );
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

    /* create the sample grabber */
    if( p_grabber )
        p_grabber->Release();
    p_grabber = NULL;
    hr = p_sample_grabber->QueryInterface( IID_ISampleGrabber,
        reinterpret_cast<void**>( &p_grabber ) );
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
            {
                msg_Dbg( p_access, "Build: "\
                    "Connected capture device to sample grabber" );
                break;
            }
            msg_Warn( p_access, "Build: "\
                "Cannot connect Sample Grabber to Capture device: hr=0x%8lx (try %d/2)", hr, 1+i );
        }
        else
        {
            msg_Warn( p_access, "Build: "\
                "Cannot set media type on grabber filter: hr=0x%8lx (try %d/2", hr, 1+i );
        }
    }
    msg_Dbg( p_access, "Build: This is where it used to return upon success" );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot use capture device: hr=0x%8lx", hr );
        return hr;
    }

    /* We need the MPEG2 Demultiplexer even though we are going to use the VLC
     * TS demuxer. The TIF filter connects to the MPEG2 demux and works with
     * the Network Provider filter to set up the stream */
    //msg_Dbg( p_access, "Build: using MPEG2 demux" );
    if( p_mpeg_demux )
        p_mpeg_demux->Release();
    p_mpeg_demux = NULL;
    hr = ::CoCreateInstance( CLSID_MPEG2Demultiplexer, NULL,
        CLSCTX_INPROC_SERVER, IID_IBaseFilter,
        reinterpret_cast<void**>( &p_mpeg_demux ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot CoCreateInstance MPEG2 Demultiplexer: hr=0x%8lx", hr );
        return hr;
    }

    //msg_Dbg( p_access, "Build: adding demux" );
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

    //msg_Dbg( p_access, "Build: Connected sample grabber to demux" );
    /* Always look for the Transport Information Filter from the start
     * of the collection*/
    l_tif_used = -1;
    msg_Dbg( p_access, "Check: "\
        "Calling FindFilter for KSCATEGORY_BDA_TRANSPORT_INFORMATION with "\
        "p_mpeg_demux; l_tif_used=%ld", l_tif_used );


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

    /* Second parameter to SetCallback specifies the callback method; 0 uses
     * the ISampleGrabberCB::SampleCB method, which receives an IMediaSample
     * pointer. */
    //msg_Dbg( p_access, "Build: Adding grabber callback" );
    hr = p_grabber->SetCallback( this, 0 );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot set SampleGrabber Callback: hr=0x%8lx", hr );
        return hr;
    }

    hr = Register(); /* creates d_graph_register */
    if( FAILED( hr ) )
    {
        d_graph_register = 0;
        msg_Dbg( p_access, "Build: "\
            "Cannot register graph: hr=0x%8lx", hr );
    }

    /* The Media Control is used to Run and Stop the Graph */
    if( p_media_control )
        p_media_control->Release();
    p_media_control = NULL;
    hr = p_filter_graph->QueryInterface( IID_IMediaControl,
        reinterpret_cast<void**>( &p_media_control ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot QI Media Control: hr=0x%8lx", hr );
        return hr;
    }

    /* success! */
    //msg_Dbg( p_access, "Build: succeeded: hr=0x%8lx", hr );
    return S_OK;
}

/* debugging */
HRESULT BDAGraph::ListFilters( REFCLSID this_clsid )
{
    HRESULT                 hr = S_OK;

    class localComPtr
    {
        public:
        ICreateDevEnum*    p_local_system_dev_enum;
        IEnumMoniker*      p_moniker_enum;
        IMoniker*          p_moniker;
        IBaseFilter*       p_filter;
        IBaseFilter*       p_this_filter;
        IBindCtx*          p_bind_context;
        IPropertyBag*      p_property_bag;

        char*              psz_downstream;
        char*              psz_bstr;
        int                i_bstr_len;

        localComPtr():
            p_local_system_dev_enum(NULL),
            p_moniker_enum(NULL),
            p_moniker(NULL),
            p_filter(NULL),
            p_this_filter(NULL),
            p_bind_context( NULL ),
            p_property_bag(NULL),
            psz_downstream( NULL ),
            psz_bstr( NULL )
        {}
        ~localComPtr()
        {
            if( p_property_bag )
                p_property_bag->Release();
            if( p_bind_context )
                p_bind_context->Release();
            if( p_filter )
                p_filter->Release();
            if( p_this_filter )
                p_this_filter->Release();
            if( p_moniker )
                p_moniker->Release();
            if( p_moniker_enum )
                p_moniker_enum->Release();
            if( p_local_system_dev_enum )
                p_local_system_dev_enum->Release();
            if( psz_bstr )
                delete[] psz_bstr;
            if( psz_downstream )
                delete[] psz_downstream;
        }
    } l;


//    msg_Dbg( p_access, "ListFilters: Create local system_dev_enum");
    if( l.p_local_system_dev_enum )
        l.p_local_system_dev_enum->Release();
    l.p_local_system_dev_enum = NULL;
    hr = ::CoCreateInstance( CLSID_SystemDeviceEnum, 0, CLSCTX_INPROC,
        IID_ICreateDevEnum, reinterpret_cast<void**>( &l.p_local_system_dev_enum ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "ListFilters: "\
            "Cannot CoCreate SystemDeviceEnum: hr=0x%8lx", hr );
        return hr;
    }

    //msg_Dbg( p_access, "ListFilters: Create p_moniker_enum");
    if( l.p_moniker_enum )
        l.p_moniker_enum->Release();
    l.p_moniker_enum = NULL;
    hr = l.p_local_system_dev_enum->CreateClassEnumerator( this_clsid,
        &l.p_moniker_enum, 0 );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "ListFilters: "\
            "Cannot CreateClassEnumerator: hr=0x%8lx", hr );
        return hr;
    }

    //msg_Dbg( p_access, "ListFilters: Entering main loop" );
    do
    {
        /* We are overwriting l.p_moniker so we should Release and nullify
         * It is important that p_moniker and p_property_bag are fully released
         * l.p_filter may not be dereferenced so we could force to NULL */
        /* msg_Dbg( p_access, "ListFilters: top of main loop");*/
        //msg_Dbg( p_access, "ListFilters: releasing property bag");
        if( l.p_property_bag )
            l.p_property_bag->Release();
        l.p_property_bag = NULL;
        //msg_Dbg( p_access, "ListFilters: releasing filter");
        if( l.p_filter )
            l.p_filter->Release();
        l.p_filter = NULL;
        //msg_Dbg( p_access, "ListFilters: releasing bind context");
        if( l.p_bind_context )
           l.p_bind_context->Release();
        l.p_bind_context = NULL;
        //msg_Dbg( p_access, "ListFilters: releasing moniker");
        if( l.p_moniker )
            l.p_moniker->Release();
        l.p_moniker = NULL;
        //msg_Dbg( p_access, "ListFilters: trying a moniker");

        if( !l.p_moniker_enum ) break;
        hr = l.p_moniker_enum->Next( 1, &l.p_moniker, 0 );
        if( hr != S_OK ) break;

        /* l.p_bind_context is Released at the top of the loop */
        hr = CreateBindCtx( 0, &l.p_bind_context );
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "ListFilters: "\
                "Cannot create bind_context, trying another: hr=0x%8lx", hr );
            continue;
        }

        /* l.p_filter is Released at the top of the loop */
        hr = l.p_moniker->BindToObject( l.p_bind_context, NULL, IID_IBaseFilter,
            reinterpret_cast<void**>( &l.p_filter ) );
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "ListFilters: "\
                "Cannot create p_filter, trying another: hr=0x%8lx", hr );
            continue;
        }

#ifdef DEBUG_MONIKER_NAME
        WCHAR*  pwsz_downstream = NULL;

        hr = l.p_moniker->GetDisplayName(l.p_bind_context, NULL,
            &pwsz_downstream );
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "ListFilters: "\
                "Cannot get display name, trying another: hr=0x%8lx", hr );
            continue;
        }

        if( l.psz_downstream )
            delete[] l.psz_downstream;
        l.i_bstr_len = WideCharToMultiByte( CP_ACP, 0, pwsz_downstream, -1,
            l.psz_downstream, 0, NULL, NULL );
        l.psz_downstream = new char[ l.i_bstr_len ];
        l.i_bstr_len = WideCharToMultiByte( CP_ACP, 0, pwsz_downstream, -1,
            l.psz_downstream, l.i_bstr_len, NULL, NULL );

        LPMALLOC p_alloc;
        ::CoGetMalloc( 1, &p_alloc );
        p_alloc->Free( pwsz_downstream );
        p_alloc->Release();
        msg_Dbg( p_access, "ListFilters: "\
            "Moniker name is  %s",  l.psz_downstream );
#else
        l.psz_downstream = strdup( "Downstream" );
#endif
        /* l.p_property_bag is released at the top of the loop */
        hr = l.p_moniker->BindToStorage( NULL, NULL, IID_IPropertyBag,
            reinterpret_cast<void**>( &l.p_property_bag ) );
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "ListFilters: "\
                "Cannot Bind to Property Bag: hr=0x%8lx", hr );
            continue;
        }

        //msg_Dbg( p_access, "ListFilters: displaying another" );
    }
    while( true );

    return S_OK;
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
HRESULT BDAGraph::FindFilter( REFCLSID this_clsid, long* i_moniker_used,
    IBaseFilter* p_upstream, IBaseFilter** p_p_downstream )
{
    HRESULT                 hr = S_OK;
    int                     i_moniker_index = -1;
    class localComPtr
    {
        public:
        IEnumMoniker*  p_moniker_enum;
        IMoniker*      p_moniker;
        IBindCtx*      p_bind_context;
        IPropertyBag*  p_property_bag;
        char*          psz_upstream;
        int            i_upstream_len;

        char*          psz_downstream;
        VARIANT        var_bstr;
        int            i_bstr_len;
        localComPtr():
            p_moniker_enum(NULL),
            p_moniker(NULL),
            p_bind_context( NULL ),
            p_property_bag(NULL),
            psz_upstream( NULL ),
            psz_downstream( NULL )
        {
            ::VariantInit(&var_bstr);
        }
        ~localComPtr()
        {
            if( p_moniker_enum )
                p_moniker_enum->Release();
            if( p_moniker )
                p_moniker->Release();
            if( p_bind_context )
                p_bind_context->Release();
            if( p_property_bag )
                p_property_bag->Release();
            if( psz_upstream )
                delete[] psz_upstream;
            if( psz_downstream )
                delete[] psz_downstream;

            ::VariantClear(&var_bstr);
        }
    } l;

    /* create system_dev_enum the first time through, or preserve the
     * existing one to loop through classes */
    if( !p_system_dev_enum )
    {
        msg_Dbg( p_access, "FindFilter: Create p_system_dev_enum");
        hr = ::CoCreateInstance( CLSID_SystemDeviceEnum, 0, CLSCTX_INPROC,
            IID_ICreateDevEnum, reinterpret_cast<void**>( &p_system_dev_enum ) );
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "FindFilter: "\
                "Cannot CoCreate SystemDeviceEnum: hr=0x%8lx", hr );
            return hr;
        }
    }

    msg_Dbg( p_access, "FindFilter: Create p_moniker_enum");
    hr = p_system_dev_enum->CreateClassEnumerator( this_clsid,
        &l.p_moniker_enum, 0 );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "FindFilter: "\
            "Cannot CreateClassEnumerator: hr=0x%8lx", hr );
        return hr;
    }

    msg_Dbg( p_access, "FindFilter: get filter name");
    hr = GetFilterName( p_upstream, &l.psz_upstream );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "FindFilter: "\
            "Cannot GetFilterName: hr=0x%8lx", hr );
        return hr;
    }

    msg_Dbg( p_access, "FindFilter: "\
        "called with i_moniker_used=%ld, " \
        "p_upstream = %s", *i_moniker_used, l.psz_upstream );

    do
    {
        /* We are overwriting l.p_moniker so we should Release and nullify
         * It is important that p_moniker and p_property_bag are fully released */
        msg_Dbg( p_access, "FindFilter: top of main loop");
        if( l.p_property_bag )
            l.p_property_bag->Release();
        l.p_property_bag = NULL;
        msg_Dbg( p_access, "FindFilter: releasing bind context");
        if( l.p_bind_context )
           l.p_bind_context->Release();
        l.p_bind_context = NULL;
        msg_Dbg( p_access, "FindFilter: releasing moniker");
        if( l.p_moniker )
            l.p_moniker->Release();
        msg_Dbg( p_access, "FindFilter: null moniker");
        l.p_moniker = NULL;

        msg_Dbg( p_access, "FindFilter: quit if no enum");
        if( !l.p_moniker_enum ) break;
        msg_Dbg( p_access, "FindFilter: trying a moniker");
        hr = l.p_moniker_enum->Next( 1, &l.p_moniker, 0 );
        if( hr != S_OK ) break;

        i_moniker_index++;

        /* Skip over devices already found on previous calls */
        msg_Dbg( p_access, "FindFilter: skip previously found devices");

        if( i_moniker_index <= *i_moniker_used ) continue;
        *i_moniker_used = i_moniker_index;

        /* l.p_bind_context is Released at the top of the loop */
        msg_Dbg( p_access, "FindFilter: create bind context");
        hr = CreateBindCtx( 0, &l.p_bind_context );
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "FindFilter: "\
                "Cannot create bind_context, trying another: hr=0x%8lx", hr );
            continue;
        }

        msg_Dbg( p_access, "FindFilter: try to create downstream filter");
        *p_p_downstream = NULL;
        hr = l.p_moniker->BindToObject( l.p_bind_context, NULL, IID_IBaseFilter,
            reinterpret_cast<void**>( p_p_downstream ) );
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "FindFilter: "\
                "Cannot bind to downstream, trying another: hr=0x%8lx", hr );
            continue;
        }

#ifdef DEBUG_MONIKER_NAME
        msg_Dbg( p_access, "FindFilter: get downstream filter name");

        WCHAR*  pwsz_downstream = NULL;

        hr = l.p_moniker->GetDisplayName(l.p_bind_context, NULL,
            &pwsz_downstream );
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "FindFilter: "\
                "Cannot get display name, trying another: hr=0x%8lx", hr );
            continue;
        }

        if( l.psz_downstream )
            delete[] l.psz_downstream;
        l.i_bstr_len = WideCharToMultiByte( CP_ACP, 0, pwsz_downstream, -1,
            l.psz_downstream, 0, NULL, NULL );
        l.psz_downstream = new char[ l.i_bstr_len ];
        l.i_bstr_len = WideCharToMultiByte( CP_ACP, 0, pwsz_downstream, -1,
            l.psz_downstream, l.i_bstr_len, NULL, NULL );

        LPMALLOC p_alloc;
        ::CoGetMalloc( 1, &p_alloc );
        p_alloc->Free( pwsz_downstream );
        p_alloc->Release();
#else
        l.psz_downstream = strdup( "Downstream" );
#endif

        /* l.p_property_bag is released at the top of the loop */
        msg_Dbg( p_access, "FindFilter: "\
            "Moniker name is  %s, binding to storage",  l.psz_downstream );
        hr = l.p_moniker->BindToStorage( l.p_bind_context, NULL,
            IID_IPropertyBag, reinterpret_cast<void**>( &l.p_property_bag ) );
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "FindFilter: "\
                "Cannot Bind to Property Bag: hr=0x%8lx", hr );
            continue;
        }

        msg_Dbg( p_access, "FindFilter: read friendly name");
        hr = l.p_property_bag->Read( L"FriendlyName", &l.var_bstr, NULL );
        if( FAILED( hr ) )
        {
           msg_Dbg( p_access, "FindFilter: "\
               "Cannot read friendly name, next?: hr=0x%8lx", hr );
           continue;
        }

        msg_Dbg( p_access, "FindFilter: add filter to graph" );
        hr = p_filter_graph->AddFilter( *p_p_downstream, l.var_bstr.bstrVal );
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "FindFilter: "\
                "Cannot add filter, trying another: hr=0x%8lx", hr );
            continue;
        }

        msg_Dbg( p_access, "FindFilter: "\
            "Trying to Connect %s to %s", l.psz_upstream, l.psz_downstream );
        hr = Connect( p_upstream, *p_p_downstream );
        if( SUCCEEDED( hr ) )
        {
            msg_Dbg( p_access, "FindFilter: Connected %s", l.psz_downstream );
            return S_OK;
        }

        /* Not the filter we want so unload and try the next one */
        /* Warning: RemoveFilter does an undocumented Release()
         * on pointer but does not set it to NULL */
        msg_Dbg( p_access, "FindFilter: Removing filter" );
        hr = p_filter_graph->RemoveFilter( *p_p_downstream );
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "FindFilter: "\
                "Failed unloading Filter: hr=0x%8lx", hr );
            continue;
        }
        msg_Dbg( p_access, "FindFilter: trying another" );
    }
    while( true );

    /* nothing found */
    msg_Warn( p_access, "FindFilter: No filter connected" );
    hr = E_FAIL;
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
        char*      psz_upstream;
        char*      psz_downstream;

        localComPtr():
            p_pin_upstream(NULL), p_pin_downstream(NULL),
            p_pin_upstream_enum(NULL), p_pin_downstream_enum(NULL),
            p_pin_temp(NULL),
            psz_upstream( NULL ),
            psz_downstream( NULL )
            { };
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
            if( psz_upstream )
                delete[] psz_upstream;
            if( psz_downstream )
                delete[] psz_downstream;
        }
    } l;

    PIN_DIRECTION pin_dir;

    //msg_Dbg( p_access, "Connect: entering" );
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
        if( l.p_pin_upstream )
            l.p_pin_upstream ->Release();
        l.p_pin_upstream = NULL;
        if( !l.p_pin_upstream_enum ) break;
        hr = l.p_pin_upstream_enum->Next( 1, &l.p_pin_upstream, 0 );
        if( hr != S_OK ) break;

        //msg_Dbg( p_access, "Connect: get pin name");
        hr = GetPinName( l.p_pin_upstream, &l.psz_upstream );
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "Connect: "\
                "Cannot GetPinName: hr=0x%8lx", hr );
            return hr;
        }
        //msg_Dbg( p_access, "Connect: p_pin_upstream = %s", l.psz_upstream );

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
            //msg_Dbg( p_access, "Connect: enumerating downstream pins" );
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
                if( l.p_pin_downstream )
                    l.p_pin_downstream ->Release();
                l.p_pin_downstream = NULL;
                if( !l.p_pin_downstream_enum ) break;
                hr = l.p_pin_downstream_enum->Next( 1, &l.p_pin_downstream, 0 );
                if( hr != S_OK ) break;

                //msg_Dbg( p_access, "Connect: get pin name");
                hr = GetPinName( l.p_pin_downstream, &l.psz_downstream );
                if( FAILED( hr ) )
                {
                    msg_Warn( p_access, "Connect: "\
                        "Cannot GetPinName: hr=0x%8lx", hr );
                    return hr;
                }
/*
                msg_Dbg( p_access, "Connect: Trying p_downstream = %s",
                    l.psz_downstream );
*/

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
                    //msg_Dbg( p_access, "Connect: trying to connect pins" );

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
    //msg_Dbg( p_access, "Connect: No pins connected" );
    return E_FAIL;
}

/*****************************************************************************
* Start uses MediaControl to start the graph
*****************************************************************************/
HRESULT BDAGraph::Start()
{
    HRESULT hr = S_OK;
    OAFilterState i_state; /* State_Stopped, State_Paused, State_Running */

    msg_Dbg( p_access, "Start: entering" );

    if( !p_media_control )
    {
        msg_Warn( p_access, "Start: Media Control has not been created" );
        return E_FAIL;
    }

    msg_Dbg( p_access, "Start: Run()" );
    hr = p_media_control->Run();
    if( SUCCEEDED( hr ) )
    {
        msg_Dbg( p_access, "Start: Graph started, hr=0x%lx", hr );
        return S_OK;
    }

    msg_Dbg( p_access, "Start: would not start, will retry" );
    /* Query the state of the graph - timeout after 100 milliseconds */
    while( (hr = p_media_control->GetState( 100, &i_state) ) != S_OK )
    {
        if( FAILED( hr ) )
        {
            msg_Warn( p_access,
                "Start: Cannot get Graph state: hr=0x%8lx", hr );
            return hr;
        }
    }

    msg_Dbg( p_access, "Start: got state" );
    if( i_state == State_Running )
    {
        msg_Dbg( p_access, "Graph started after a delay, hr=0x%lx", hr );
        return S_OK;
    }

    /* The Graph is not running so stop it and return an error */
    msg_Warn( p_access, "Start: Graph not started: %d", (int)i_state );
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
ssize_t BDAGraph::Pop(void *buf, size_t len)
{
    return output.Pop(buf, len);
}

/******************************************************************************
* SampleCB - Callback when the Sample Grabber has a sample
******************************************************************************/
STDMETHODIMP BDAGraph::SampleCB( double /*date*/, IMediaSample *p_sample )
{
    if( p_sample->IsDiscontinuity() == S_OK )
        msg_Warn( p_access, "BDA SampleCB: Sample Discontinuity.");

    const size_t i_sample_size = p_sample->GetActualDataLength();

    /* The buffer memory is owned by the media sample object, and is automatically
     * released when the media sample is destroyed. The caller should not free or
     * reallocate the buffer. */
    BYTE *p_sample_data;
    p_sample->GetPointer( &p_sample_data );

    if( i_sample_size > 0 && p_sample_data )
    {
        block_t *p_block = block_Alloc( i_sample_size );

        if( p_block )
        {
            memcpy( p_block->p_buffer, p_sample_data, i_sample_size );
            output.Push( p_block );
        }
     }
     return S_OK;
}

STDMETHODIMP BDAGraph::BufferCB( double /*date*/, BYTE* /*buffer*/,
                                 long /*buffer_len*/ )
{
    return E_FAIL;
}

/******************************************************************************
* removes each filter from the graph
******************************************************************************/
HRESULT BDAGraph::Destroy()
{
    HRESULT hr = S_OK;
    ULONG mem_ref = 0;
//    msg_Dbg( p_access, "Destroy: media control 1" );
    if( p_media_control )
        p_media_control->StopWhenReady(); /* Instead of Stop() */

//    msg_Dbg( p_access, "Destroy: deregistering graph" );
    if( d_graph_register )
        Deregister();

//    msg_Dbg( p_access, "Destroy: calling Empty" );
    output.Empty();

//    msg_Dbg( p_access, "Destroy: TIF" );
    if( p_transport_info )
    {
        /* Warning: RemoveFilter does an undocumented Release()
         * on pointer but does not set it to NULL */
        hr = p_filter_graph->RemoveFilter( p_transport_info );
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "Destroy: "\
                "Failed unloading TIF: hr=0x%8lx", hr );
        }
        p_transport_info = NULL;
    }

//    msg_Dbg( p_access, "Destroy: demux" );
    if( p_mpeg_demux )
    {
        p_filter_graph->RemoveFilter( p_mpeg_demux );
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "Destroy: "\
                "Failed unloading demux: hr=0x%8lx", hr );
        }
        p_mpeg_demux = NULL;
    }

//    msg_Dbg( p_access, "Destroy: sample grabber" );
    if( p_grabber )
    {
        mem_ref = p_grabber->Release();
        if( mem_ref != 0 )
        {
            msg_Dbg( p_access, "Destroy: "\
                "Sample grabber mem_ref (varies): mem_ref=%ld", mem_ref );
        }
        p_grabber = NULL;
    }

//    msg_Dbg( p_access, "Destroy: sample grabber filter" );
    if( p_sample_grabber )
    {
        hr = p_filter_graph->RemoveFilter( p_sample_grabber );
        p_sample_grabber = NULL;
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "Destroy: "\
                "Failed unloading sampler: hr=0x%8lx", hr );
        }
    }

//    msg_Dbg( p_access, "Destroy: capture device" );
    if( p_capture_device )
    {
        p_filter_graph->RemoveFilter( p_capture_device );
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "Destroy: "\
                "Failed unloading capture device: hr=0x%8lx", hr );
        }
        p_capture_device = NULL;
    }

//    msg_Dbg( p_access, "Destroy: tuner device" );
    if( p_tuner_device )
    {
        //msg_Dbg( p_access, "Destroy: remove filter on tuner device" );
        hr = p_filter_graph->RemoveFilter( p_tuner_device );
        //msg_Dbg( p_access, "Destroy: force tuner device to NULL" );
        p_tuner_device = NULL;
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "Destroy: "\
                "Failed unloading tuner device: hr=0x%8lx", hr );
        }
    }

//    msg_Dbg( p_access, "Destroy: scanning tuner" );
    if( p_scanning_tuner )
    {
        mem_ref = p_scanning_tuner->Release();
        if( mem_ref != 0 )
        {
            msg_Dbg( p_access, "Destroy: "\
                "Scanning tuner mem_ref (normally 2 if warm, "\
                "3 if active): mem_ref=%ld", mem_ref );
        }
        p_scanning_tuner = NULL;
    }

//    msg_Dbg( p_access, "Destroy: net provider" );
    if( p_network_provider )
    {
        hr = p_filter_graph->RemoveFilter( p_network_provider );
        p_network_provider = NULL;
        if( FAILED( hr ) )
        {
            msg_Dbg( p_access, "Destroy: "\
                "Failed unloading net provider: hr=0x%8lx", hr );
        }
    }

//    msg_Dbg( p_access, "Destroy: filter graph" );
    if( p_filter_graph )
    {
        mem_ref = p_filter_graph->Release();
        if( mem_ref != 0 )
        {
            msg_Dbg( p_access, "Destroy: "\
                "Filter graph mem_ref (normally 1 if active): mem_ref=%ld",
                mem_ref );
        }
        p_filter_graph = NULL;
    }

    /* first call to FindFilter creates p_system_dev_enum */

//    msg_Dbg( p_access, "Destroy: system dev enum" );
    if( p_system_dev_enum )
    {
        mem_ref = p_system_dev_enum->Release();
        if( mem_ref != 0 )
        {
            msg_Dbg( p_access, "Destroy: "\
                "System_dev_enum mem_ref: mem_ref=%ld", mem_ref );
        }
        p_system_dev_enum = NULL;
    }

//    msg_Dbg( p_access, "Destroy: media control 2" );
    if( p_media_control )
    {
        msg_Dbg( p_access, "Destroy: release media control" );
        mem_ref = p_media_control->Release();
        if( mem_ref != 0 )
        {
            msg_Dbg( p_access, "Destroy: "\
                "Media control mem_ref: mem_ref=%ld", mem_ref );
        }
        msg_Dbg( p_access, "Destroy: force media control to NULL" );
        p_media_control = NULL;
    }

    d_graph_register = 0;
    l_tuner_used = -1;
    guid_network_type = GUID_NULL;

//    msg_Dbg( p_access, "Destroy: returning" );
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
        localComPtr():
            p_moniker(NULL),
            p_ro_table(NULL)
        {};
        ~localComPtr()
        {
            if( p_moniker )
                p_moniker->Release();
            if( p_ro_table )
                p_ro_table->Release();
        }
    } l;
    WCHAR     pwsz_graph_name[128];
    HRESULT   hr;

    hr = ::GetRunningObjectTable( 0, &l.p_ro_table );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Register: Cannot get ROT: hr=0x%8lx", hr );
        return hr;
    }

    size_t len = sizeof(pwsz_graph_name) / sizeof(pwsz_graph_name[0]);
    _snwprintf( pwsz_graph_name, len - 1, L"VLC BDA Graph %08x Pid %08x",
        (DWORD_PTR) p_filter_graph, ::GetCurrentProcessId() );
    pwsz_graph_name[len-1] = 0;
    hr = CreateItemMoniker( L"!", pwsz_graph_name, &l.p_moniker );
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

    msg_Dbg( p_access, "Register: registered Graph: %S", pwsz_graph_name );
    return hr;
}

void BDAGraph::Deregister()
{
    HRESULT   hr;
    IRunningObjectTable* p_ro_table;
    hr = ::GetRunningObjectTable( 0, &p_ro_table );
    /* docs say this does a Release() on d_graph_register stuff */
    if( SUCCEEDED( hr ) )
        p_ro_table->Revoke( d_graph_register );
    d_graph_register = 0;
    p_ro_table->Release();
}

HRESULT BDAGraph::GetFilterName( IBaseFilter* p_filter, char** psz_bstr_name )
{
    FILTER_INFO filter_info;
    HRESULT     hr = S_OK;

    hr = p_filter->QueryFilterInfo(&filter_info);
    if( FAILED( hr ) )
        return hr;
    int i_name_len = WideCharToMultiByte( CP_ACP, 0, filter_info.achName,
        -1, *psz_bstr_name, 0, NULL, NULL );
    *psz_bstr_name = new char[ i_name_len ];
    i_name_len = WideCharToMultiByte( CP_ACP, 0, filter_info.achName,
        -1, *psz_bstr_name, i_name_len, NULL, NULL );

    // The FILTER_INFO structure holds a pointer to the Filter Graph
    // Manager, with a reference count that must be released.
    if( filter_info.pGraph )
        filter_info.pGraph->Release();
    return S_OK;
}

HRESULT BDAGraph::GetPinName( IPin* p_pin, char** psz_bstr_name )
{
    PIN_INFO    pin_info;
    HRESULT     hr = S_OK;

    hr = p_pin->QueryPinInfo(&pin_info);
    if( FAILED( hr ) )
        return hr;
    int i_name_len = WideCharToMultiByte( CP_ACP, 0, pin_info.achName,
        -1, *psz_bstr_name, 0, NULL, NULL );
    *psz_bstr_name = new char[ i_name_len ];
    i_name_len = WideCharToMultiByte( CP_ACP, 0, pin_info.achName,
        -1, *psz_bstr_name, i_name_len, NULL, NULL );

    // The PIN_INFO structure holds a pointer to the Filter,
    // with a referenppce count that must be released.
    if( pin_info.pFilter )
        pin_info.pFilter->Release();
    return S_OK;
}
