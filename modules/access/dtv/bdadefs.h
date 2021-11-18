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

#include <tuner.h>

class ISampleGrabber;
class ISampleGrabberCB;
class IMpeg2Data;
class IGuideData;
class IGuideDataEvent;
class ISectionList;
class IEnumTuneRequests;
class IEnumGuideDataProperties;
class IGuideDataProperty;
class IMpeg2Stream;


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

const CLSID CLSID_DigitalCableNetworkType =
    {0x143827AB,0xF77B,0x498d,{0x81,0xCA,0x5A,0x00,0x7A,0xEC,0x28,0xBF}};

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

extern const CLSID CLSID_SampleGrabber; // found in strmiids

const IID IID_IMpeg2Data =
    {0x9B396D40,0xF380,0x4e3c,{0xA5,0x14,0x1A,0x82,0xBF,0x6E,0xBF,0xE6}};

};
