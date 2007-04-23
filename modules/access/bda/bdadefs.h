/*****************************************************************************
 * bdadefs.h : DirectShow BDA headers for vlc
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 *
 * Author: Ken Self <kens@campoz.fslife.co.uk>
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

class IATSCChannelTuneRequest;
class IATSCLocator;
class IChannelTuneRequest;
class IComponent;
class IComponents;
class IComponentType;
class IComponentTypes;
class IDVBCLocator;
class IDVBSLocator;
class IDVBTLocator;
class IDVBTuneRequest;
class IEnumComponents;
class IEnumComponentTypes;
class IEnumTuningSpaces;
class ILocator;
class ISampleGrabber;
class ISampleGrabberCB;
class IScanningTuner;
class ITuner;
class ITuneRequest;
class ITuningSpace;
class ITuningSpaceContainer;
class ITuningSpaces;

typedef enum BinaryConvolutionCodeRate
{
    BDA_BCC_RATE_NOT_SET = -1,
    BDA_BCC_RATE_NOT_DEFINED=0,
    BDA_BCC_RATE_1_2 = 1,
    BDA_BCC_RATE_2_3,
    BDA_BCC_RATE_3_4,
    BDA_BCC_RATE_3_5,
    BDA_BCC_RATE_4_5,
    BDA_BCC_RATE_5_6,
    BDA_BCC_RATE_5_11,
    BDA_BCC_RATE_7_8,
    BDA_BCC_RATE_MAX,
} BinaryConvolutionCodeRate;

typedef enum ComponentCategory
{
    CategoryNotSet = -1,
    CategoryOther=0,
    CategoryVideo,
    CategoryAudio,
    CategoryText,
    CategoryData,
} ComponentCategory;

typedef enum ComponentStatus
{
    StatusActive,
    StatusInactive,
    StatusUnavailable,
} ComponentStatus;

typedef enum FECMethod
{
    BDA_FEC_METHOD_NOT_SET = -1,
    BDA_FEC_METHOD_NOT_DEFINED=0,
    BDA_FEC_VITERBI = 1,
    BDA_FEC_RS_204_188,
    BDA_FEC_MAX,
} FECMethod;

typedef enum GuardInterval
{
    BDA_GUARD_NOT_SET = -1,
    BDA_GUARD_NOT_DEFINED=0,
    BDA_GUARD_1_32 = 1,
    BDA_GUARD_1_16,
    BDA_GUARD_1_8,
    BDA_GUARD_1_4,
    BDA_GUARD_MAX,
} GuardInterval;

typedef enum HierarchyAlpha
{
    BDA_HALPHA_NOT_SET = -1,
    BDA_HALPHA_NOT_DEFINED=0,
    BDA_HALPHA_1 = 1,
    BDA_HALPHA_2,
    BDA_HALPHA_4,
    BDA_HALPHA_MAX,
} HierarchyAlpha;

typedef enum ModulationType
{
    BDA_MOD_NOT_SET = -1,
    BDA_MOD_NOT_DEFINED=0,
    BDA_MOD_16QAM = 1,
    BDA_MOD_32QAM,
    BDA_MOD_64QAM,
    BDA_MOD_80QAM,
    BDA_MOD_96QAM,
    BDA_MOD_112QAM,
    BDA_MOD_128QAM,
    BDA_MOD_160QAM,
    BDA_MOD_192QAM,
    BDA_MOD_224QAM,
    BDA_MOD_256QAM,
    BDA_MOD_320QAM,
    BDA_MOD_384QAM,
    BDA_MOD_448QAM,
    BDA_MOD_512QAM,
    BDA_MOD_640QAM,
    BDA_MOD_768QAM,
    BDA_MOD_896QAM,
    BDA_MOD_1024QAM,
    BDA_MOD_QPSK,
    BDA_MOD_BPSK,
    BDA_MOD_OQPSK,
    BDA_MOD_8VSB,
    BDA_MOD_16VSB,
    BDA_MOD_ANALOG_AMPLITUDE,
    BDA_MOD_ANALOG_FREQUENCY,
    BDA_MOD_MAX,
} ModulationType;

typedef enum Polarisation
{
    BDA_POLARISATION_NOT_SET     = -1,
    BDA_POLARISATION_NOT_DEFINED = 0,
    BDA_POLARISATION_LINEAR_H    = 1,
    BDA_POLARISATION_LINEAR_V    = 2,
    BDA_POLARISATION_CIRCULAR_L  = 3,
    BDA_POLARISATION_CIRCULAR_R  = 4,
    BDA_POLARISATION_MAX         = 5
} Polarisation;

typedef enum TransmissionMode
{
    BDA_XMIT_MODE_NOT_SET = -1,
    BDA_XMIT_MODE_NOT_DEFINED=0,
    BDA_XMIT_MODE_2K = 1,
    BDA_XMIT_MODE_8K,
    BDA_XMIT_MODE_MAX,
} TransmissionMode;

class IComponent : public IDispatch
{
public:
    virtual HRESULT __stdcall get_Type( IComponentType** p_p_cpt_type )=0;
    virtual HRESULT __stdcall put_Type( IComponentType* p_cpt_type )=0;
    virtual HRESULT __stdcall get_DescLangID( long* p_l_language )=0;
    virtual HRESULT __stdcall put_DescLangID( long l_language )=0;
    virtual HRESULT __stdcall get_Status( ComponentStatus* p_status )=0;
    virtual HRESULT __stdcall put_Status( ComponentStatus status )=0;
    virtual HRESULT __stdcall get_Description( BSTR* p_bstr_desc )=0;
    virtual HRESULT __stdcall put_Description( BSTR bstr_desc )=0;
    virtual HRESULT __stdcall Clone( IComponent** p_p_component )=0;
};

class IComponents : public IDispatch
{
public:
    virtual HRESULT __stdcall get_Count( long* pl_count )=0;
    virtual HRESULT __stdcall get__NewEnum( IEnumVARIANT** p_p_enum )=0;
    virtual HRESULT __stdcall EnumComponents( IEnumComponents** p_p_enum )=0;
    virtual HRESULT __stdcall get_Item( VARIANT Index,
        IComponent** p_p_component )=0;
    virtual HRESULT __stdcall Add( IComponent* p_component,
        VARIANT* v_index )=0;
    virtual HRESULT __stdcall Remove( VARIANT v_index )=0;
    virtual HRESULT __stdcall Clone( IComponents** p_p_cpts )=0;
};

class IComponentType : public IDispatch
{
public:
    virtual HRESULT __stdcall get_Category( ComponentCategory* p_category )=0;
    virtual HRESULT __stdcall put_Category( ComponentCategory category )=0;
    virtual HRESULT __stdcall get_MediaMajorType( BSTR* p_bstr_major_type )=0;
    virtual HRESULT __stdcall put_MediaMajorType( BSTR bstr_major_type )=0;
    virtual HRESULT __stdcall get__MediaMajorType( GUID* p_guid_major_type )=0;
    virtual HRESULT __stdcall put__MediaMajorType( REFCLSID guid_major_type )=0;
    virtual HRESULT __stdcall get_MediaSubType( BSTR* p_bstr_sub_type )=0;
    virtual HRESULT __stdcall put_MediaSubType( BSTR bstr_sub_type )=0;
    virtual HRESULT __stdcall get__MediaSubType( GUID* p_guid_sub_type )=0;
    virtual HRESULT __stdcall put__MediaSubType( REFCLSID guid_sub_type )=0;
    virtual HRESULT __stdcall get_MediaFormatType( BSTR* p_bstr_format_type )=0;
    virtual HRESULT __stdcall put_MediaFormatType( BSTR bstr_format_type )=0;
    virtual HRESULT __stdcall get__MediaFormatType(
        GUID* p_guid_format_type )=0;
    virtual HRESULT __stdcall put__MediaFormatType(
        REFCLSID guid_format_type )=0;
    virtual HRESULT __stdcall get_MediaType( AM_MEDIA_TYPE* p_media_type )=0;
    virtual HRESULT __stdcall put_MediaType( AM_MEDIA_TYPE* p_media_type )=0;
    virtual HRESULT __stdcall Clone( IComponentType** p_p_cpt_type )=0;
};

class IComponentTypes : public IDispatch
{
public:
    virtual HRESULT __stdcall get_Count( long* l_count )=0;
    virtual HRESULT __stdcall get__NewEnum( IEnumVARIANT** p_p_enum )=0;
    virtual HRESULT __stdcall EnumComponentTypes(
        IEnumComponentTypes** p_p_enum )=0;
    virtual HRESULT __stdcall get_Item( VARIANT v_index,
        IComponentType** p_p_cpt_type )=0;
    virtual HRESULT __stdcall put_Item( VARIANT v_index,
        IComponentType* p_cpt_type )=0;
    virtual HRESULT __stdcall Add( IComponentType* p_cpt_type,
        VARIANT* v_index )=0;
    virtual HRESULT __stdcall Remove( VARIANT v_index )=0;
    virtual HRESULT __stdcall Clone( IComponentTypes** p_p_cpt_types )=0;
};

class IEnumComponents : public IUnknown
{
public:
    virtual HRESULT __stdcall Next( ULONG num_elem, IComponent** p_p_elem,
        ULONG* p_num_elem_fetch )=0;
    virtual HRESULT __stdcall Skip( ULONG num_elem )=0;
    virtual HRESULT __stdcall Reset( void )=0;
    virtual HRESULT __stdcall Clone( IEnumComponents** p_p_enum )=0;
};

class IEnumComponentTypes : public IUnknown
{
public:
    virtual HRESULT __stdcall Next( ULONG num_elem, IComponentType** p_p_elem,
        ULONG* p_num_elem_fetch )=0;
    virtual HRESULT __stdcall Skip( ULONG num_elem )=0;
    virtual HRESULT __stdcall Reset( void )=0;
    virtual HRESULT __stdcall Clone( IEnumComponentTypes** p_p_enum )=0;
};

class IEnumTuningSpaces : public IUnknown
{
public:
    virtual HRESULT __stdcall Next( ULONG l_num_elem,
        ITuningSpace** p_p_tuning_space, ULONG* pl_num_elem_fetched )=0;
    virtual HRESULT __stdcall Skip( ULONG l_num_elem )=0;
    virtual HRESULT __stdcall Reset( void )=0;
    virtual HRESULT __stdcall Clone( IEnumTuningSpaces** p_p_enum )=0;
};

class ITuner : public IUnknown
{
public:
    virtual HRESULT __stdcall get_TuningSpace(
        ITuningSpace** p_p_tuning_space )=0;
    virtual HRESULT __stdcall put_TuningSpace( ITuningSpace* p_tuning_space )=0;
    virtual HRESULT __stdcall EnumTuningSpaces( IEnumTuningSpaces** p_p_enum )=0;
    virtual HRESULT __stdcall get_TuneRequest(
        ITuneRequest** p_p_tune_request )=0;
    virtual HRESULT __stdcall put_TuneRequest( ITuneRequest* p_tune_request )=0;
    virtual HRESULT __stdcall Validate( ITuneRequest* p_tune_request )=0;
    virtual HRESULT __stdcall get_PreferredComponentTypes(
        IComponentTypes** p_p_cpt_types )=0;
    virtual HRESULT __stdcall put_PreferredComponentTypes(
        IComponentTypes* p_cpt_types )=0;
    virtual HRESULT __stdcall get_SignalStrength( long* l_sig_strength )=0;
    virtual HRESULT __stdcall TriggerSignalEvents( long l_interval )=0;
};

class ISampleGrabber : public IUnknown
{
public:
    virtual HRESULT __stdcall SetOneShot( BOOL b_one_shot )=0;
    virtual HRESULT __stdcall SetMediaType(
        const AM_MEDIA_TYPE* p_media_type )=0;
    virtual HRESULT __stdcall GetConnectedMediaType(
        AM_MEDIA_TYPE* p_media_type )=0;
    virtual HRESULT __stdcall SetBufferSamples( BOOL b_buffer_samples )=0;
    virtual HRESULT __stdcall GetCurrentBuffer( long* p_buff_size,
        long* p_buffer )=0;
    virtual HRESULT __stdcall GetCurrentSample( IMediaSample** p_p_sample )=0;
    virtual HRESULT __stdcall SetCallback( ISampleGrabberCB* pf_callback,
        long l_callback_type )=0;
};

class ISampleGrabberCB : public IUnknown
{
public:
    virtual HRESULT __stdcall SampleCB( double d_sample_time,
        IMediaSample* p_sample )=0;
    virtual HRESULT __stdcall BufferCB( double d_sample_time, BYTE *p_buffer,
        long l_bufferLen )=0;
};

class IScanningTuner : public ITuner
{
public:
    virtual HRESULT __stdcall SeekUp( void )=0;
    virtual HRESULT __stdcall SeekDown( void )=0;
    virtual HRESULT __stdcall ScanDown( long l_pause )=0;
    virtual HRESULT __stdcall ScanUp( long l_pause )=0;
    virtual HRESULT __stdcall AutoProgram( void )=0;
};

class ITuneRequest : public IDispatch
{
public:
    virtual HRESULT __stdcall get_TuningSpace(
        ITuningSpace** p_p_tuning_space )=0;
    virtual HRESULT __stdcall get_Components( IComponents** p_p_components )=0;
    virtual HRESULT __stdcall Clone( ITuneRequest** p_p_tune_request )=0;
    virtual HRESULT __stdcall get_Locator( ILocator** p_p_locator )=0;
    virtual HRESULT __stdcall put_Locator( ILocator* p_locator )=0;
};

class IChannelTuneRequest : public ITuneRequest
{
public:
    virtual HRESULT __stdcall get_Channel( long* pl_channel )=0;
    virtual HRESULT __stdcall put_Channel( long l_channel )=0;
};

class IATSCChannelTuneRequest : public IChannelTuneRequest
{
public:
    virtual HRESULT __stdcall get_MinorChannel( long* pl_minor_channel )=0;
    virtual HRESULT __stdcall put_MinorChannel( long l_minor_channel )=0;
};

class IDVBTuneRequest : public ITuneRequest
{
public:
    virtual HRESULT __stdcall get_ONID( long* pl_onid )=0;
    virtual HRESULT __stdcall put_ONID( long l_onid )=0;
    virtual HRESULT __stdcall get_TSID( long* pl_tsid )=0;
    virtual HRESULT __stdcall put_TSID( long l_tsid )=0;
    virtual HRESULT __stdcall get_SID( long* pl_sid )=0;
    virtual HRESULT __stdcall put_SID( long l_sid )=0;
};

class ILocator : public IDispatch
{
public:
    virtual HRESULT __stdcall get_CarrierFrequency( long* pl_frequency )=0;
    virtual HRESULT __stdcall put_CarrierFrequency( long l_frequency )=0;
    virtual HRESULT __stdcall get_InnerFEC( FECMethod* FEC )=0;
    virtual HRESULT __stdcall put_InnerFEC( FECMethod FEC )=0;
    virtual HRESULT __stdcall get_InnerFECRate(
        BinaryConvolutionCodeRate* FEC )=0;
    virtual HRESULT __stdcall put_InnerFECRate(
        BinaryConvolutionCodeRate FEC )=0;
    virtual HRESULT __stdcall get_OuterFEC( FECMethod* FEC )=0;
    virtual HRESULT __stdcall put_OuterFEC( FECMethod FEC )=0;
    virtual HRESULT __stdcall get_OuterFECRate(
        BinaryConvolutionCodeRate* FEC )=0;
    virtual HRESULT __stdcall put_OuterFECRate(
        BinaryConvolutionCodeRate FEC )=0;
    virtual HRESULT __stdcall get_Modulation( ModulationType* p_modulation )=0;
    virtual HRESULT __stdcall put_Modulation( ModulationType modulation )=0;
    virtual HRESULT __stdcall get_SymbolRate( long* pl_rate )=0;
    virtual HRESULT __stdcall put_SymbolRate( long l_rate )=0;
    virtual HRESULT __stdcall Clone( ILocator** p_p_locator )=0;
};

class IATSCLocator : public ILocator
{
public:
    virtual HRESULT __stdcall get_PhysicalChannel( long* pl_phys_channel )=0;
    virtual HRESULT __stdcall put_PhysicalChannel( long l_phys_channel )=0;
    virtual HRESULT __stdcall get_TSID( long* pl_tsid )=0;
    virtual HRESULT __stdcall put_TSID( long l_tsid )=0;
};

class IDVBCLocator : public ILocator
{
public:
};

class IDVBSLocator : public ILocator
{
public:
    virtual HRESULT __stdcall get_SignalPolarisation(
        Polarisation* p_polarisation )=0;
    virtual HRESULT __stdcall put_SignalPolarisation(
        Polarisation polarisation )=0;
    virtual HRESULT __stdcall get_WestPosition( VARIANT_BOOL* pb_west )=0;
    virtual HRESULT __stdcall put_WestPosition( VARIANT_BOOL b_west )=0;
    virtual HRESULT __stdcall get_OrbitalPosition( long* pl_longitude )=0;
    virtual HRESULT __stdcall put_OrbitalPosition( long l_longitude )=0;
    virtual HRESULT __stdcall get_Azimuth( long* pl_azimuth )=0;
    virtual HRESULT __stdcall put_Azimuth( long l_azimuth )=0;
    virtual HRESULT __stdcall get_Elevation( long* pl_elevation )=0;
    virtual HRESULT __stdcall put_Elevation( long l_elevation )=0;
};

class IDVBTLocator : public ILocator
{
public:
    virtual HRESULT __stdcall get_Bandwidth( long* pl_bandwidth )=0;
    virtual HRESULT __stdcall put_Bandwidth( long l_bandwidth )=0;
    virtual HRESULT __stdcall get_LPInnerFEC( FECMethod* FEC )=0;
    virtual HRESULT __stdcall put_LPInnerFEC( FECMethod FEC )=0;
    virtual HRESULT __stdcall get_LPInnerFECRate(
        BinaryConvolutionCodeRate* FEC )=0;
    virtual HRESULT __stdcall put_LPInnerFECRate(
        BinaryConvolutionCodeRate FEC )=0;
    virtual HRESULT __stdcall get_HAlpha( HierarchyAlpha* Alpha )=0;
    virtual HRESULT __stdcall put_HAlpha( HierarchyAlpha Alpha )=0;
    virtual HRESULT __stdcall get_Guard( GuardInterval* GI )=0;
    virtual HRESULT __stdcall put_Guard( GuardInterval GI )=0;
    virtual HRESULT __stdcall get_Mode( TransmissionMode* mode )=0;
    virtual HRESULT __stdcall put_Mode( TransmissionMode mode )=0;
    virtual HRESULT __stdcall get_OtherFrequencyInUse(
        VARIANT_BOOL* OtherFrequencyInUseVal )=0;
    virtual HRESULT __stdcall put_OtherFrequencyInUse(
        VARIANT_BOOL OtherFrequencyInUseVal )=0;
};

class ITuningSpace : public IDispatch
{
public:
    virtual HRESULT __stdcall get_UniqueName( BSTR* p_bstr_name )=0;
    virtual HRESULT __stdcall put_UniqueName( BSTR Name )=0;
    virtual HRESULT __stdcall get_FriendlyName( BSTR* p_bstr_name )=0;
    virtual HRESULT __stdcall put_FriendlyName( BSTR bstr_name )=0;
    virtual HRESULT __stdcall get_CLSID( BSTR* bstr_clsid )=0;
    virtual HRESULT __stdcall get_NetworkType( BSTR* p_bstr_network_guid )=0;
    virtual HRESULT __stdcall put_NetworkType( BSTR bstr_network_guid )=0;
    virtual HRESULT __stdcall get__NetworkType( GUID* p_guid_network_guid )=0;
    virtual HRESULT __stdcall put__NetworkType( REFCLSID clsid_network_guid )=0;
    virtual HRESULT __stdcall CreateTuneRequest(
        ITuneRequest** p_p_tune_request )=0;
    virtual HRESULT __stdcall EnumCategoryGUIDs( IEnumGUID** p_p_enum )=0;
    virtual HRESULT __stdcall EnumDeviceMonikers( IEnumMoniker** p_p_enum )=0;
    virtual HRESULT __stdcall get_DefaultPreferredComponentTypes(
        IComponentTypes** p_p_cpt_types )=0;
    virtual HRESULT __stdcall put_DefaultPreferredComponentTypes(
        IComponentTypes* p_cpt_types )=0;
    virtual HRESULT __stdcall get_FrequencyMapping( BSTR* p_bstr_mapping )=0;
    virtual HRESULT __stdcall put_FrequencyMapping( BSTR bstr_mapping )=0;
    virtual HRESULT __stdcall get_DefaultLocator( ILocator** p_p_locator )=0;
    virtual HRESULT __stdcall put_DefaultLocator( ILocator* p_locator )=0;
    virtual HRESULT __stdcall Clone( ITuningSpace** p_p_tuning_space )=0;
};

class ITuningSpaceContainer : public IDispatch
{
public:
    virtual HRESULT __stdcall get_Count( long* l_count )=0;
    virtual HRESULT __stdcall get__NewEnum( IEnumVARIANT** p_p_enum )=0;
    virtual HRESULT __stdcall get_Item( VARIANT v_index,
        ITuningSpace** p_p_tuning_space )=0;
    virtual HRESULT __stdcall put_Item( VARIANT v_index,
        ITuningSpace* p_tuning_space )=0;
    virtual HRESULT __stdcall TuningSpacesForCLSID( BSTR bstr_clsid,
        ITuningSpaces** p_p_tuning_spaces )=0;
    virtual HRESULT __stdcall _TuningSpacesForCLSID( REFCLSID clsid,
        ITuningSpaces** p_p_tuning_spaces )=0;
    virtual HRESULT __stdcall TuningSpacesForName( BSTR bstr_name,
        ITuningSpaces** p_p_tuning_spaces )=0;
    virtual HRESULT __stdcall FindID( ITuningSpace* p_tuning_space,
        long* l_id )=0;
    virtual HRESULT __stdcall Add( ITuningSpace* p_tuning_space,
        VARIANT* v_index )=0;
    virtual HRESULT __stdcall get_EnumTuningSpaces(
        IEnumTuningSpaces** p_p_enum )=0;
    virtual HRESULT __stdcall Remove( VARIANT v_index )=0;
    virtual HRESULT __stdcall get_MaxCount( long* l_maxcount )=0;
    virtual HRESULT __stdcall put_MaxCount( long l_maxcount )=0;
};

class ITuningSpaces : public IDispatch
{
public:
    virtual HRESULT __stdcall get_Count( long* l_count )=0;
    virtual HRESULT __stdcall get__NewEnum( IEnumVARIANT** p_p_enum )=0;
    virtual HRESULT __stdcall get_Item( VARIANT v_index,
        ITuningSpace** p_p_tuning_space )=0;
    virtual HRESULT __stdcall get_EnumTuningSpaces(
        IEnumTuningSpaces** p_p_enum )=0;
};
