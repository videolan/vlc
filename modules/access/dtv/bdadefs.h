/*****************************************************************************
 * bdadefs.h : DirectShow BDA headers for vlc
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
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

class IDigitalCableTuneRequest;
class IDigitalCableLocator;
class IATSCChannelTuneRequest;
class IATSCLocator;
class IBDA_DeviceControl;
class IBDA_FrequencyFilter;
class IBDA_SignalStatistics;
class IBDA_Topology;
class IChannelTuneRequest;
class IComponent;
class IComponents;
class IComponentType;
class IComponentTypes;
class IDVBCLocator;
class IDVBSLocator;
class IDVBSTuningSpace;
class IDVBTLocator;
class IDVBTLocator2;
class IDVBTuneRequest;
class IDVBTuningSpace;
class IDVBTuningSpace2;
class IEnumComponents;
class IEnumComponentTypes;
class IEnumTuningSpaces;
class ILocator;
class ISampleGrabber;
class ISampleGrabberCB;
class IScanningTuner;
class ITuner;
class ITunerCap;
class ITuneRequest;
class ITuningSpace;
class ITuningSpaceContainer;
class ITuningSpaces;
class IMpeg2Data;
class IGuideData;
class IGuideDataEvent;
class ISectionList;
class IEnumTuneRequests;
class IEnumGuideDataProperties;
class IGuideDataProperty;
class IMpeg2Stream;

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

typedef enum DVBSystemType
{
    DVB_Cable,
    DVB_Terrestrial,
    DVB_Satellite,
} DVBSystemType;

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

typedef enum SpectralInversion
{
    BDA_SPECTRAL_INVERSION_NOT_SET = -1,
    BDA_SPECTRAL_INVERSION_NOT_DEFINED = 0,
    BDA_SPECTRAL_INVERSION_AUTOMATIC = 1,
    BDA_SPECTRAL_INVERSION_NORMAL,
    BDA_SPECTRAL_INVERSION_INVERTED,
    BDA_SPECTRAL_INVERSION_MAX
} SpectralInversion;

typedef enum TransmissionMode
{
    BDA_XMIT_MODE_NOT_SET = -1,
    BDA_XMIT_MODE_NOT_DEFINED=0,
    BDA_XMIT_MODE_2K = 1,
    BDA_XMIT_MODE_8K,
    BDA_XMIT_MODE_MAX,
} TransmissionMode;

typedef struct _BDANODE_DESCRIPTOR
{
    ULONG               ulBdaNodeType;
    GUID                guidFunction;
    GUID                guidName;
} BDANODE_DESCRIPTOR, *PBDANODE_DESCRIPTOR;

typedef struct _BDA_TEMPLATE_CONNECTION
{
    ULONG   FromNodeType;
    ULONG   FromNodePinType;
    ULONG   ToNodeType;
    ULONG   ToNodePinType;
} BDA_TEMPLATE_CONNECTION, *PBDA_TEMPLATE_CONNECTION;

typedef struct _BDA_TEMPLATE_PIN_JOINT
{
    ULONG   uliTemplateConnection;
    ULONG   ulcInstancesMax;
} BDA_TEMPLATE_PIN_JOINT, *PBDA_TEMPLATE_PIN_JOINT;

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

class ITunerCap : public IUnknown
{
public:
    virtual HRESULT __stdcall get_AuxInputCount( ULONG* pulCompositeCount,
        ULONG* pulSvideoCount )=0;
    virtual HRESULT __stdcall get_SupportedNetworkTypes(
        ULONG ulcNetworkTypesMax, ULONG* pulcNetworkTypes,
        GUID* pguidNetworkTypes )=0;
    virtual HRESULT __stdcall get_SupportedVideoFormats(
        ULONG* pulAMTunerModeType, ULONG* pulAnalogVideoStandard )=0;
};


class ITuner : public IUnknown
{
public:
    virtual HRESULT __stdcall get_TuningSpace(
        ITuningSpace** p_p_tuning_space )=0;
    virtual HRESULT __stdcall put_TuningSpace( ITuningSpace* p_tuning_space )=0;
    virtual HRESULT __stdcall EnumTuningSpaces(
       IEnumTuningSpaces** p_p_enum )=0;
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

class IDigitalCableTuneRequest : public IATSCChannelTuneRequest
{
public:
    virtual HRESULT __stdcall get_MajorChannel( long* pl_major_channel )=0;
    virtual HRESULT __stdcall put_MajorChannel( long l_major_channel )=0;
    virtual HRESULT __stdcall get_SourceID( long* pl_source_id )=0;
    virtual HRESULT __stdcall put_SourceID( long l_source_id )=0;
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

class IATSCLocator2 : public IATSCLocator
{
public:
    virtual HRESULT __stdcall get_ProgramNumber( long* pl_prog_number )=0;
    virtual HRESULT __stdcall put_ProgramNumber( long l_prog_number )=0;
};

class IDigitalCableLocator : public IATSCLocator2
{
public:
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

class IDVBTLocator2 : public IDVBTLocator
{
public:
    virtual HRESULT __stdcall get_PhysicalLayerPipeId( long* pl_plp )=0;
    virtual HRESULT __stdcall put_PhysicalLayerPipeId( long l_plp )=0;
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

class IDVBTuningSpace : public ITuningSpace
{
public:
    virtual HRESULT __stdcall get_SystemType( DVBSystemType* p_sys_type )=0;
    virtual HRESULT __stdcall put_SystemType( DVBSystemType sys_type )=0;
};

class IDVBTuningSpace2 : public IDVBTuningSpace
{
public:
    virtual HRESULT __stdcall get_NetworkID( long* p_l_network_id )=0;
    virtual HRESULT __stdcall put_NetworkID( long l_network_id )=0;
};

class IDVBSTuningSpace : public IDVBTuningSpace2
{
public:
    virtual HRESULT __stdcall get_LowOscillator( long* p_l_low_osc )=0;
    virtual HRESULT __stdcall put_LowOscillator( long l_low_osc )=0;
    virtual HRESULT __stdcall get_HighOscillator( long* p_l_high_osc )=0;
    virtual HRESULT __stdcall put_HighOscillator( long l_high_osc )=0;
    virtual HRESULT __stdcall get_LNBSwitch( long* p_l_lnb_switch )=0;
    virtual HRESULT __stdcall put_LNBSwitch( long l_lnb_switch )=0;
    virtual HRESULT __stdcall get_InputRange( BSTR* p_bstr_input_range )=0;
    virtual HRESULT __stdcall put_InputRange( BSTR bstr_input_range )=0;
    virtual HRESULT __stdcall get_SpectralInversion(
        SpectralInversion* p_spectral_inv )=0;
    virtual HRESULT __stdcall put_SpectralInversion(
        SpectralInversion spectral_inv )=0;
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

class IBDA_DeviceControl : public IUnknown
{
public:
    virtual HRESULT __stdcall StartChanges( void )=0;
    virtual HRESULT __stdcall CheckChanges( void )=0;
    virtual HRESULT __stdcall CommitChanges( void )=0;
    virtual HRESULT __stdcall GetChangeState( ULONG *pState )=0;
};

class IBDA_FrequencyFilter : public IUnknown
{
public:
    virtual HRESULT __stdcall put_Autotune( ULONG ulTransponder )=0;
    virtual HRESULT __stdcall get_Autotune( ULONG *pulTransponder )=0;
    virtual HRESULT __stdcall put_Frequency( ULONG ulFrequency )=0;
    virtual HRESULT __stdcall get_Frequency( ULONG *pulFrequency )=0;
    virtual HRESULT __stdcall put_Polarity( Polarisation Polarity )=0;
    virtual HRESULT __stdcall get_Polarity( Polarisation *pPolarity )=0;
    virtual HRESULT __stdcall put_Range( ULONG ulRange )=0;
    virtual HRESULT __stdcall get_Range( ULONG *pulRange )=0;
    virtual HRESULT __stdcall put_Bandwidth( ULONG ulBandwidth )=0;
    virtual HRESULT __stdcall get_Bandwidth( ULONG *pulBandwidth )=0;
    virtual HRESULT __stdcall put_FrequencyMultiplier( ULONG ulMultiplier )=0;
    virtual HRESULT __stdcall get_FrequencyMultiplier(
        ULONG *pulMultiplier )=0;
};

class IBDA_SignalStatistics : public IUnknown
{
public:
    virtual HRESULT __stdcall put_SignalStrength( LONG lDbStrength )=0;
    virtual HRESULT __stdcall get_SignalStrength( LONG *plDbStrength )=0;
    virtual HRESULT __stdcall put_SignalQuality( LONG lPercentQuality )=0;
    virtual HRESULT __stdcall get_SignalQuality( LONG *plPercentQuality )=0;
    virtual HRESULT __stdcall put_SignalPresent( BOOLEAN fPresent )=0;
    virtual HRESULT __stdcall get_SignalPresent( BOOLEAN *pfPresent )=0;
    virtual HRESULT __stdcall put_SignalLocked( BOOLEAN fLocked )=0;
    virtual HRESULT __stdcall get_SignalLocked( BOOLEAN *pfLocked )=0;
    virtual HRESULT __stdcall put_SampleTime( LONG lmsSampleTime )=0;
    virtual HRESULT __stdcall get_SampleTime( LONG *plmsSampleTime )=0;
};

class IBDA_Topology : public IUnknown
{
public:
    virtual HRESULT __stdcall GetNodeTypes( ULONG *pulcNodeTypes,
        ULONG ulcNodeTypesMax, ULONG rgulNodeTypes[] )=0;
    virtual HRESULT __stdcall GetNodeDescriptors( ULONG *ulcNodeDescriptors,
        ULONG ulcNodeDescriptorsMax,
        BDANODE_DESCRIPTOR rgNodeDescriptors[] )=0;
    virtual HRESULT __stdcall GetNodeInterfaces( ULONG ulNodeType,
        ULONG *pulcInterfaces, ULONG ulcInterfacesMax,
        GUID rgguidInterfaces[] )=0;
    virtual HRESULT __stdcall GetPinTypes( ULONG *pulcPinTypes,
        ULONG ulcPinTypesMax, ULONG rgulPinTypes[] )=0;
    virtual HRESULT __stdcall GetTemplateConnections( ULONG *pulcConnections,
        ULONG ulcConnectionsMax, BDA_TEMPLATE_CONNECTION rgConnections[] )=0;
    virtual HRESULT __stdcall CreatePin( ULONG ulPinType, ULONG *pulPinId )=0;
    virtual HRESULT __stdcall DeletePin( ULONG ulPinId )=0;
    virtual HRESULT __stdcall SetMediaType( ULONG ulPinId,
       AM_MEDIA_TYPE *pMediaType )=0;
    virtual HRESULT __stdcall SetMedium( ULONG ulPinId,
       REGPINMEDIUM *pMedium )=0;
    virtual HRESULT __stdcall CreateTopology( ULONG ulInputPinId,
       ULONG ulOutputPinId )=0;
    virtual HRESULT __stdcall GetControlNode( ULONG ulInputPinId,
       ULONG ulOutputPinId, ULONG ulNodeType, IUnknown **ppControlNode )=0;
};

typedef struct _MPEG_HEADER_BITS_MIDL
{
    WORD Bits;
} MPEG_HEADER_BITS_MIDL;

typedef struct _MPEG_HEADER_VERSION_BITS_MIDL
{
    BYTE Bits;
} MPEG_HEADER_VERSION_BITS_MIDL;

typedef WORD PID;

typedef BYTE TID;

typedef struct _SECTION
{
    TID TableId;
    union
    {
        MPEG_HEADER_BITS_MIDL S;
        WORD W;
    } Header;
    BYTE SectionData[ 1 ];
} SECTION, *PSECTION;

typedef struct _LONG_SECTION
{
    TID TableId;
    union
    {
        MPEG_HEADER_BITS_MIDL S;
        WORD W;
    } Header;
    WORD TableIdExtension;
    union
    {
        MPEG_HEADER_VERSION_BITS_MIDL S;
        BYTE B;
        } Version;
    BYTE SectionNumber;
    BYTE LastSectionNumber;
    BYTE RemainingData[ 1 ];
} LONG_SECTION;

typedef struct _MPEG_BCS_DEMUX
{
    DWORD AVMGraphId;
} MPEG_BCS_DEMUX;

typedef struct _MPEG_WINSOC
{
    DWORD AVMGraphId;
} MPEG_WINSOCK;

typedef enum
{
    MPEG_CONTEXT_BCS_DEMUX = 0,
    MPEG_CONTEXT_WINSOCK = MPEG_CONTEXT_BCS_DEMUX + 1
} MPEG_CONTEXT_TYPE;

typedef struct _MPEG_RQST_PACKET
{
    DWORD dwLength;
    PSECTION pSection;
} MPEG_RQST_PACKET, *PMPEG_RQST_PACKET;

typedef struct _MPEG_PACKET_LIST
{
    WORD wPacketCount;
    PMPEG_RQST_PACKET PacketList[ 1 ];
} MPEG_PACKET_LIST, *PMPEG_PACKET_LIST;

typedef struct _DSMCC_FILTER_OPTIONS
{
    BOOL fSpecifyProtocol;
    BYTE Protocol;
    BOOL fSpecifyType;
    BYTE Type;
    BOOL fSpecifyMessageId;
    WORD MessageId;
    BOOL fSpecifyTransactionId;
    BOOL fUseTrxIdMessageIdMask;
    DWORD TransactionId;
    BOOL fSpecifyModuleVersion;
    BYTE ModuleVersion;
    BOOL fSpecifyBlockNumber;
    WORD BlockNumber;
    BOOL fGetModuleCall;
    WORD NumberOfBlocksInModule;
} DSMCC_FILTER_OPTIONS;

typedef struct _ATSC_FILTER_OPTIONS
{
    BOOL fSpecifyEtmId;
    DWORD EtmId;
} ATSC_FILTER_OPTIONS;

typedef struct _MPEG_STREAM_BUFFER
{
    HRESULT hr;
    DWORD dwDataBufferSize;
    DWORD dwSizeOfDataRead;
    BYTE *pDataBuffer;
} MPEG_STREAM_BUFFER, *PMPEG_STREAM_BUFFER;

typedef struct _MPEG_CONTEXT
{
    MPEG_CONTEXT_TYPE Type;
    union
    {
        MPEG_BCS_DEMUX Demux;
        MPEG_WINSOCK Winsock;
    } U;
} MPEG_CONTEXT, *PMPEG_CONTEXT;

typedef enum
{
   MPEG_RQST_UNKNOWN = 0,
   MPEG_RQST_GET_SECTION = MPEG_RQST_UNKNOWN + 1,
   MPEG_RQST_GET_SECTION_ASYNC = MPEG_RQST_GET_SECTION + 1,
   MPEG_RQST_GET_TABLE = MPEG_RQST_GET_SECTION_ASYNC + 1,
   MPEG_RQST_GET_TABLE_ASYNC = MPEG_RQST_GET_TABLE + 1,
   MPEG_RQST_GET_SECTIONS_STREAM = MPEG_RQST_GET_TABLE_ASYNC + 1,
   MPEG_RQST_GET_PES_STREAM = MPEG_RQST_GET_SECTIONS_STREAM + 1,
   MPEG_RQST_GET_TS_STREAM = MPEG_RQST_GET_PES_STREAM + 1,
   MPEG_RQST_START_MPE_STREAM = MPEG_RQST_GET_TS_STREAM + 1
} MPEG_REQUEST_TYPE;

typedef struct _MPEG2_FILTER
{
    BYTE bVersionNumber;
    WORD wFilterSize;
    BOOL fUseRawFilteringBits;
    BYTE Filter[ 16 ];
    BYTE Mask[ 16 ];
    BOOL fSpecifyTableIdExtension;
    WORD TableIdExtension;
    BOOL fSpecifyVersion;
    BYTE Version;
    BOOL fSpecifySectionNumber;
    BYTE SectionNumber;
    BOOL fSpecifyCurrentNext;
    BOOL fNext;
    BOOL fSpecifyDsmccOptions;
    DSMCC_FILTER_OPTIONS Dsmcc;
    BOOL fSpecifyAtscOptions;
    ATSC_FILTER_OPTIONS Atsc;
} MPEG2_FILTER, *PMPEG2_FILTER;

typedef struct _MPEG_HEADER_BITS
{
    WORD SectionLength          : 12;
    WORD Reserved               :  2;
    WORD PrivateIndicator       :  1;
    WORD SectionSyntaxIndicator :  1;
} MPEG_HEADER_BITS, *PMPEG_HEADER_BITS;

typedef struct _MPEG_HEADER_VERSION_BITS
{
    BYTE CurrentNextIndicator : 1;
    BYTE VersionNumber        : 5;
    BYTE Reserved             : 2;
} MPEG_HEADER_VERSION_BITS, *PMPEG_HEADER_VERSION_BITS;

class IMpeg2Data : public IUnknown
{
public:
    virtual HRESULT __stdcall GetSection( PID pid, TID tid,
        PMPEG2_FILTER pFilter, DWORD dwTimeout,
        ISectionList **ppSectionList )=0;
    virtual HRESULT __stdcall GetTable( PID pid, TID tid, PMPEG2_FILTER pFilter,
        DWORD dwTimeout, ISectionList **ppSectionList )=0;
    virtual HRESULT __stdcall GetStreamOfSections( PID pid, TID tid,
        PMPEG2_FILTER pFilter, HANDLE hDataReadyEvent,
        IMpeg2Stream **ppMpegStream )=0;
};

class IGuideData : public IUnknown
{
public:
    virtual HRESULT __stdcall GetServices(
        IEnumTuneRequests **ppEnumTuneRequestslass )=0;
    virtual HRESULT __stdcall GetServiceProperties(
        ITuneRequest *pTuneRequest,
        IEnumGuideDataProperties **ppEnumProperties )=0;
    virtual HRESULT __stdcall GetGuideProgramIDs(
        IEnumVARIANT **pEnumPrograms )=0;
    virtual HRESULT __stdcall GetProgramProperties(
        VARIANT varProgramDescriptionID,
        IEnumGuideDataProperties **ppEnumProperties )=0;
    virtual HRESULT __stdcall GetScheduleEntryIDs(
        IEnumVARIANT **pEnumScheduleEntries )=0;
    virtual HRESULT __stdcall GetScheduleEntryProperties(
        VARIANT varScheduleEntryDescriptionID,
        IEnumGuideDataProperties **ppEnumProperties )=0;
};

class IGuideDataEvent : public IUnknown
{
public:
    virtual HRESULT __stdcall GuideDataAcquired( void )=0;
    virtual HRESULT __stdcall ProgramChanged(
        VARIANT varProgramDescriptionID )=0;
    virtual HRESULT __stdcall ServiceChanged(
        VARIANT varServiceDescriptionID )=0;
    virtual HRESULT __stdcall ScheduleEntryChanged(
        VARIANT varScheduleEntryDescriptionID )=0;
    virtual HRESULT __stdcall ProgramDeleted(
        VARIANT varProgramDescriptionID )=0;
    virtual HRESULT __stdcall ServiceDeleted(
        VARIANT varServiceDescriptionID )=0;
    virtual HRESULT __stdcall ScheduleDeleted(
            VARIANT varScheduleEntryDescriptionID )=0;
};

class IGuideDataProperty : public IUnknown
{
public:
    virtual  HRESULT __stdcall get_Name( BSTR *pbstrName )=0;
    virtual  HRESULT __stdcall get_Language( long *idLang )=0;
    virtual  HRESULT __stdcall get_Value( VARIANT *pvar )=0;
};

class IMpeg2Stream : public IUnknown
{
public:
    virtual HRESULT __stdcall Initialize( MPEG_REQUEST_TYPE requestType,
        IMpeg2Data *pMpeg2Data, PMPEG_CONTEXT pContext, PID pid, TID tid,
        PMPEG2_FILTER pFilter, HANDLE hDataReadyEvent )=0;
    virtual HRESULT __stdcall SupplyDataBuffer(
        PMPEG_STREAM_BUFFER pStreamBuffer )=0;
};

class ISectionList : public IUnknown
{
public:
    virtual HRESULT __stdcall Initialize( MPEG_REQUEST_TYPE requestType,
        IMpeg2Data *pMpeg2Data, PMPEG_CONTEXT pContext, PID pid, TID tid,
        PMPEG2_FILTER pFilter, DWORD timeout, HANDLE hDoneEvent )=0;
    virtual HRESULT __stdcall InitializeWithRawSections(
        PMPEG_PACKET_LIST pmplSections )=0;
    virtual HRESULT __stdcall CancelPendingRequest( void )=0;
    virtual HRESULT __stdcall GetNumberOfSections( WORD *pCount )=0;
    virtual HRESULT __stdcall GetSectionData( WORD sectionNumber,
        DWORD *pdwRawPacketLength, PSECTION *ppSection )=0;
    virtual HRESULT __stdcall GetProgramIdentifier( PID *pPid )=0;
    virtual HRESULT __stdcall GetTableIdentifier( TID *pTableId )=0;
};

class IEnumGuideDataProperties : public IUnknown
{
public:
    virtual HRESULT __stdcall Next( unsigned long celt,
        IGuideDataProperty **ppprop, unsigned long *pcelt )=0;
    virtual HRESULT __stdcall Skip( unsigned long celt )=0;
    virtual HRESULT __stdcall Reset( void )=0;
    virtual HRESULT __stdcall Clone( IEnumGuideDataProperties **ppenum )=0;
};

class IEnumTuneRequests : public IUnknown
{
public:
    virtual HRESULT __stdcall Next( unsigned long celt, ITuneRequest **ppprop,
        unsigned long *pcelt )=0;
    virtual HRESULT __stdcall Skip( unsigned long celt )=0;
    virtual HRESULT __stdcall Reset( void )=0;
    virtual HRESULT __stdcall Clone( IEnumTuneRequests **ppenum )=0;
};

extern "C" {
/* Following GUIDs are for the new windows 7 interfaces  */
/* windows 7 universal provider applies to all networks */
const CLSID CLSID_NetworkProvider =
    {0xB2F3A67C,0x29DA,0x4C78,{0x88,0x31,0x09,0x1E,0xD5,0x09,0xA4,0x75}};

/* Win 7 - Digital Cable - North America Clear QAM */
const CLSID CLSID_DigitalCableTuningSpace =
    {0xD9BB4CEE,0xB87A,0x47F1,{0xAC,0xF1,0xB0,0x8D,0x9C,0x78,0x13,0xFC}};
const CLSID CLSID_DigitalCableLocator =
    {0x03C06416,0xD127,0x407A,{0xAB,0x4C,0xFD,0xD2,0x79,0xAB,0xBE,0x5D}};
const CLSID CLSID_DigitalCableNetworkType =
    {0x143827AB,0xF77B,0x498d,{0x81,0xCA,0x5A,0x00,0x7A,0xEC,0x28,0xBF}};
const IID IID_IDigitalCableTuneRequest =
    {0xBAD7753B,0x6B37,0x4810,{0xAE,0x57,0x3C,0xE0,0xC4,0xA9,0xE6,0xCB}};
const IID IID_IDigitalCableLocator =
    {0x48F66A11,0x171A,0x419A,{0x95,0x25,0xBE,0xEE,0xCD,0x51,0x58,0x4C}};

/* KSCATEGORY_BDA */
const GUID KSCATEGORY_BDA_NETWORK_PROVIDER =
    {0x71985F4B,0x1CA1,0x11d3,{0x9C,0xC8,0x00,0xC0,0x4F,0x79,0x71,0xE0}};
const GUID KSCATEGORY_BDA_TRANSPORT_INFORMATION =
    {0xa2e3074f,0x6c3d,0x11d3,{0xb6,0x53,0x00,0xc0,0x4f,0x79,0x49,0x8e}};
const GUID KSCATEGORY_BDA_RECEIVER_COMPONENT    =
    {0xFD0A5AF4,0xB41D,0x11d2,{0x9c,0x95,0x00,0xc0,0x4f,0x79,0x71,0xe0}};
const GUID KSCATEGORY_BDA_NETWORK_TUNER         =
    {0x71985f48,0x1ca1,0x11d3,{0x9c,0xc8,0x00,0xc0,0x4f,0x79,0x71,0xe0}};
const GUID KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT =
    {0xF4AEB342,0x0329,0x4fdd,{0xA8,0xFD,0x4A,0xFF,0x49,0x26,0xC9,0x78}};

extern const CLSID CLSID_ATSCLocator;
extern const CLSID CLSID_ATSCNetworkProvider;
extern const CLSID CLSID_ATSCTuningSpace;
extern const CLSID CLSID_DVBCLocator;
extern const CLSID CLSID_DVBCNetworkProvider;
extern const CLSID CLSID_DVBSLocator;
extern const CLSID CLSID_DVBSNetworkProvider;
extern const CLSID CLSID_DVBSTuningSpace;
extern const CLSID CLSID_DVBTuningSpace;
extern const CLSID CLSID_DVBTLocator;
/* Following symbol does not exist in library
extern const CLSID CLSID_DVBTLocator2; */
const CLSID CLSID_DVBTLocator2 =
    {0xEFE3FA02,0x45D7,0x4920,{0xBE,0x96,0x53,0xFA,0x7F,0x35,0xB0,0xE6}};
extern const CLSID CLSID_DVBTNetworkProvider;
extern const CLSID CLSID_FilterGraph;
extern const CLSID CLSID_InfTee;
extern const CLSID CLSID_MPEG2Demultiplexer;
extern const CLSID CLSID_NullRenderer;
extern const CLSID CLSID_SampleGrabber;
extern const CLSID CLSID_SystemDeviceEnum;
extern const CLSID CLSID_SystemTuningSpaces;

extern const IID IID_IATSCChannelTuneRequest;
extern const IID IID_IATSCLocator;
extern const IID IID_IBaseFilter;
extern const IID IID_IBDA_DeviceControl;
extern const IID IID_IBDA_FrequencyFilter;
extern const IID IID_IBDA_SignalStatistics;
/* Following symbol does not exist in library
extern const IID IID_IBDA_Topology; */
const IID IID_IBDA_Topology =
    {0x79B56888,0x7FEA,0x4690,{0xB4,0x5D,0x38,0xFD,0x3C,0x78,0x49,0xBE}};
extern const IID IID_ICreateDevEnum;
extern const IID IID_IDVBTLocator;
/* Following symbol does not exist in library
extern const IID IID_IDVBTLocator2; */
const IID IID_IDVBTLocator2 =
    {0x448A2EDF,0xAE95,0x4b43,{0xA3,0xCC,0x74,0x78,0x43,0xC4,0x53,0xD4}};
extern const IID IID_IDVBCLocator;
extern const IID IID_IDVBSLocator;
extern const IID IID_IDVBSTuningSpace;
extern const IID IID_IDVBTuneRequest;
extern const IID IID_IDVBTuningSpace;
extern const IID IID_IDVBTuningSpace2;
extern const IID IID_IGraphBuilder;
extern const IID IID_IMediaControl;
extern const IID IID_IMpeg2Demultiplexer;
extern const IID IID_ISampleGrabber;
extern const IID IID_IScanningTuner;
extern const IID IID_ITuner;
/* Following symbol does not exist in library
extern const IID IID_ITunerCap; */
const IID IID_ITunerCap =
    {0xE60DFA45,0x8D56,0x4e65,{0xA8,0xAB,0xD6,0xBE,0x94,0x12,0xC2,0x49}};
extern const IID IID_ITuningSpace;
extern const IID IID_ITuningSpaceContainer;
/* Following symbol does not exist in library
extern const IID IID_IMpeg2Data; */
const IID IID_IMpeg2Data =
    {0x9B396D40,0xF380,0x4e3c,{0xA5,0x14,0x1A,0x82,0xBF,0x6E,0xBF,0xE6}};
extern const IID IID_IGuideData;
extern const IID IID_ISectionList;
extern const IID IID_IEnumTuneRequests;
extern const IID IID_IEnumGuideDataProperties;
extern const IID IID_IGuideDataProperty;
extern const IID IID_IMpeg2Stream;
extern const IID IID_IGuideDataEvent;

extern const GUID MEDIATYPE_MPEG2_SECTIONS;
extern const GUID MEDIASUBTYPE_MPEG2_TRANSPORT;
extern const GUID MEDIASUBTYPE_None;
extern const GUID FORMAT_None;

};
