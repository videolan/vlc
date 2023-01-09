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

#ifndef __ISampleGrabberCB_FWD_DEFINED__
#define __ISampleGrabberCB_FWD_DEFINED__
typedef interface ISampleGrabberCB ISampleGrabberCB;
#ifdef __cplusplus
interface ISampleGrabberCB;
#endif /* __cplusplus */
#endif

#ifndef __ISampleGrabber_FWD_DEFINED__
#define __ISampleGrabber_FWD_DEFINED__
typedef interface ISampleGrabber ISampleGrabber;
#ifdef __cplusplus
interface ISampleGrabber;
#endif /* __cplusplus */
#endif

class IMpeg2Data;
class IGuideData;
class IGuideDataEvent;
class ISectionList;
class IEnumTuneRequests;
class IEnumGuideDataProperties;
class IGuideDataProperty;
class IMpeg2Stream;


/*****************************************************************************
 * ISampleGrabberCB interface
 */
#ifndef __ISampleGrabberCB_INTERFACE_DEFINED__
#define __ISampleGrabberCB_INTERFACE_DEFINED__

DEFINE_GUID(IID_ISampleGrabberCB, 0x0579154a, 0x2b53, 0x4994, 0xb0,0xd0, 0xe7,0x73,0x14,0x8e,0xff,0x85);
#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("0579154a-2b53-4994-b0d0-e773148eff85")
ISampleGrabberCB : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE SampleCB(
        double SampleTime,
        IMediaSample *pSample) = 0;

    virtual HRESULT STDMETHODCALLTYPE BufferCB(
        double SampleTime,
        BYTE *pBuffer,
        LONG BufferLen) = 0;

};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ISampleGrabberCB, 0x0579154a, 0x2b53, 0x4994, 0xb0,0xd0, 0xe7,0x73,0x14,0x8e,0xff,0x85)
#endif
#else
typedef struct ISampleGrabberCBVtbl {
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        ISampleGrabberCB *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        ISampleGrabberCB *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        ISampleGrabberCB *This);

    /*** ISampleGrabberCB methods ***/
    HRESULT (STDMETHODCALLTYPE *SampleCB)(
        ISampleGrabberCB *This,
        double SampleTime,
        IMediaSample *pSample);

    HRESULT (STDMETHODCALLTYPE *BufferCB)(
        ISampleGrabberCB *This,
        double SampleTime,
        BYTE *pBuffer,
        LONG BufferLen);

    END_INTERFACE
} ISampleGrabberCBVtbl;

interface ISampleGrabberCB {
    CONST_VTBL ISampleGrabberCBVtbl* lpVtbl;
};

#ifdef COBJMACROS
#ifndef WIDL_C_INLINE_WRAPPERS
/*** IUnknown methods ***/
#define ISampleGrabberCB_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define ISampleGrabberCB_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ISampleGrabberCB_Release(This) (This)->lpVtbl->Release(This)
/*** ISampleGrabberCB methods ***/
#define ISampleGrabberCB_SampleCB(This,SampleTime,pSample) (This)->lpVtbl->SampleCB(This,SampleTime,pSample)
#define ISampleGrabberCB_BufferCB(This,SampleTime,pBuffer,BufferLen) (This)->lpVtbl->BufferCB(This,SampleTime,pBuffer,BufferLen)
#else
/*** IUnknown methods ***/
static FORCEINLINE HRESULT ISampleGrabberCB_QueryInterface(ISampleGrabberCB* This,REFIID riid,void **ppvObject) {
    return This->lpVtbl->QueryInterface(This,riid,ppvObject);
}
static FORCEINLINE ULONG ISampleGrabberCB_AddRef(ISampleGrabberCB* This) {
    return This->lpVtbl->AddRef(This);
}
static FORCEINLINE ULONG ISampleGrabberCB_Release(ISampleGrabberCB* This) {
    return This->lpVtbl->Release(This);
}
/*** ISampleGrabberCB methods ***/
static FORCEINLINE HRESULT ISampleGrabberCB_SampleCB(ISampleGrabberCB* This,double SampleTime,IMediaSample *pSample) {
    return This->lpVtbl->SampleCB(This,SampleTime,pSample);
}
static FORCEINLINE HRESULT ISampleGrabberCB_BufferCB(ISampleGrabberCB* This,double SampleTime,BYTE *pBuffer,LONG BufferLen) {
    return This->lpVtbl->BufferCB(This,SampleTime,pBuffer,BufferLen);
}
#endif
#endif

#endif


#endif  /* __ISampleGrabberCB_INTERFACE_DEFINED__ */

/*****************************************************************************
 * ISampleGrabber interface
 */
#ifndef __ISampleGrabber_INTERFACE_DEFINED__
#define __ISampleGrabber_INTERFACE_DEFINED__

DEFINE_GUID(IID_ISampleGrabber, 0x6b652fff, 0x11fe, 0x4fce, 0x92,0xad, 0x02,0x66,0xb5,0xd7,0xc7,0x8f);
#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("6b652fff-11fe-4fce-92ad-0266b5d7c78f")
ISampleGrabber : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE SetOneShot(
        BOOL OneShot) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetMediaType(
        const AM_MEDIA_TYPE *pType) = 0;

    virtual HRESULT __stdcall GetConnectedMediaType(
        AM_MEDIA_TYPE *pType) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(
        BOOL BufferThem) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(
        LONG *pBufferSize,
        LONG *pBuffer) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(
        IMediaSample **ppSample) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetCallback(
        ISampleGrabberCB *pCallback,
        LONG WhichMethodToCallback) = 0;

};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ISampleGrabber, 0x6b652fff, 0x11fe, 0x4fce, 0x92,0xad, 0x02,0x66,0xb5,0xd7,0xc7,0x8f)
#endif
#else
typedef struct ISampleGrabberVtbl {
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        ISampleGrabber *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        ISampleGrabber *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        ISampleGrabber *This);

    /*** ISampleGrabber methods ***/
    HRESULT (STDMETHODCALLTYPE *SetOneShot)(
        ISampleGrabber *This,
        BOOL OneShot);

    HRESULT (STDMETHODCALLTYPE *SetMediaType)(
        ISampleGrabber *This,
        const AM_MEDIA_TYPE *pType);

    HRESULT (STDMETHODCALLTYPE *GetConnectedMediaType)(
        ISampleGrabber *This,
        AM_MEDIA_TYPE *pType);

    HRESULT (STDMETHODCALLTYPE *SetBufferSamples)(
        ISampleGrabber *This,
        BOOL BufferThem);

    HRESULT (STDMETHODCALLTYPE *GetCurrentBuffer)(
        ISampleGrabber *This,
        LONG *pBufferSize,
        LONG *pBuffer);

    HRESULT (STDMETHODCALLTYPE *GetCurrentSample)(
        ISampleGrabber *This,
        IMediaSample **ppSample);

    HRESULT (STDMETHODCALLTYPE *SetCallback)(
        ISampleGrabber *This,
        ISampleGrabberCB *pCallback,
        LONG WhichMethodToCallback);

    END_INTERFACE
} ISampleGrabberVtbl;

interface ISampleGrabber {
    CONST_VTBL ISampleGrabberVtbl* lpVtbl;
};

#ifdef COBJMACROS
#ifndef WIDL_C_INLINE_WRAPPERS
/*** IUnknown methods ***/
#define ISampleGrabber_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define ISampleGrabber_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ISampleGrabber_Release(This) (This)->lpVtbl->Release(This)
/*** ISampleGrabber methods ***/
#define ISampleGrabber_SetOneShot(This,OneShot) (This)->lpVtbl->SetOneShot(This,OneShot)
#define ISampleGrabber_SetMediaType(This,pType) (This)->lpVtbl->SetMediaType(This,pType)
#define ISampleGrabber_GetConnectedMediaType(This,pType) (This)->lpVtbl->GetConnectedMediaType(This,pType)
#define ISampleGrabber_SetBufferSamples(This,BufferThem) (This)->lpVtbl->SetBufferSamples(This,BufferThem)
#define ISampleGrabber_GetCurrentBuffer(This,pBufferSize,pBuffer) (This)->lpVtbl->GetCurrentBuffer(This,pBufferSize,pBuffer)
#define ISampleGrabber_GetCurrentSample(This,ppSample) (This)->lpVtbl->GetCurrentSample(This,ppSample)
#define ISampleGrabber_SetCallback(This,pCallback,WhichMethodToCallback) (This)->lpVtbl->SetCallback(This,pCallback,WhichMethodToCallback)
#else
/*** IUnknown methods ***/
static FORCEINLINE HRESULT ISampleGrabber_QueryInterface(ISampleGrabber* This,REFIID riid,void **ppvObject) {
    return This->lpVtbl->QueryInterface(This,riid,ppvObject);
}
static FORCEINLINE ULONG ISampleGrabber_AddRef(ISampleGrabber* This) {
    return This->lpVtbl->AddRef(This);
}
static FORCEINLINE ULONG ISampleGrabber_Release(ISampleGrabber* This) {
    return This->lpVtbl->Release(This);
}
/*** ISampleGrabber methods ***/
static FORCEINLINE HRESULT ISampleGrabber_SetOneShot(ISampleGrabber* This,BOOL OneShot) {
    return This->lpVtbl->SetOneShot(This,OneShot);
}
static FORCEINLINE HRESULT ISampleGrabber_SetMediaType(ISampleGrabber* This,const AM_MEDIA_TYPE *pType) {
    return This->lpVtbl->SetMediaType(This,pType);
}
static FORCEINLINE HRESULT ISampleGrabber_GetConnectedMediaType(ISampleGrabber* This,AM_MEDIA_TYPE *pType) {
    return This->lpVtbl->GetConnectedMediaType(This,pType);
}
static FORCEINLINE HRESULT ISampleGrabber_SetBufferSamples(ISampleGrabber* This,BOOL BufferThem) {
    return This->lpVtbl->SetBufferSamples(This,BufferThem);
}
static FORCEINLINE HRESULT ISampleGrabber_GetCurrentBuffer(ISampleGrabber* This,LONG *pBufferSize,LONG *pBuffer) {
    return This->lpVtbl->GetCurrentBuffer(This,pBufferSize,pBuffer);
}
static FORCEINLINE HRESULT ISampleGrabber_GetCurrentSample(ISampleGrabber* This,IMediaSample **ppSample) {
    return This->lpVtbl->GetCurrentSample(This,ppSample);
}
static FORCEINLINE HRESULT ISampleGrabber_SetCallback(ISampleGrabber* This,ISampleGrabberCB *pCallback,LONG WhichMethodToCallback) {
    return This->lpVtbl->SetCallback(This,pCallback,WhichMethodToCallback);
}
#endif
#endif

#endif


#endif  /* __ISampleGrabber_INTERFACE_DEFINED__ */


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
    virtual HRESULT STDMETHODCALLTYPE GetSection( PID pid, TID tid,
        PMPEG2_FILTER pFilter, DWORD dwTimeout,
        ISectionList **ppSectionList )=0;
    virtual HRESULT STDMETHODCALLTYPE GetTable( PID pid, TID tid, PMPEG2_FILTER pFilter,
        DWORD dwTimeout, ISectionList **ppSectionList )=0;
    virtual HRESULT STDMETHODCALLTYPE GetStreamOfSections( PID pid, TID tid,
        PMPEG2_FILTER pFilter, HANDLE hDataReadyEvent,
        IMpeg2Stream **ppMpegStream )=0;
};

class IGuideData : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetServices(
        IEnumTuneRequests **ppEnumTuneRequestslass )=0;
    virtual HRESULT STDMETHODCALLTYPE GetServiceProperties(
        ITuneRequest *pTuneRequest,
        IEnumGuideDataProperties **ppEnumProperties )=0;
    virtual HRESULT STDMETHODCALLTYPE GetGuideProgramIDs(
        IEnumVARIANT **pEnumPrograms )=0;
    virtual HRESULT STDMETHODCALLTYPE GetProgramProperties(
        VARIANT varProgramDescriptionID,
        IEnumGuideDataProperties **ppEnumProperties )=0;
    virtual HRESULT STDMETHODCALLTYPE GetScheduleEntryIDs(
        IEnumVARIANT **pEnumScheduleEntries )=0;
    virtual HRESULT STDMETHODCALLTYPE GetScheduleEntryProperties(
        VARIANT varScheduleEntryDescriptionID,
        IEnumGuideDataProperties **ppEnumProperties )=0;
};

class IGuideDataEvent : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GuideDataAcquired( void )=0;
    virtual HRESULT STDMETHODCALLTYPE ProgramChanged(
        VARIANT varProgramDescriptionID )=0;
    virtual HRESULT STDMETHODCALLTYPE ServiceChanged(
        VARIANT varServiceDescriptionID )=0;
    virtual HRESULT STDMETHODCALLTYPE ScheduleEntryChanged(
        VARIANT varScheduleEntryDescriptionID )=0;
    virtual HRESULT STDMETHODCALLTYPE ProgramDeleted(
        VARIANT varProgramDescriptionID )=0;
    virtual HRESULT STDMETHODCALLTYPE ServiceDeleted(
        VARIANT varServiceDescriptionID )=0;
    virtual HRESULT STDMETHODCALLTYPE ScheduleDeleted(
            VARIANT varScheduleEntryDescriptionID )=0;
};

class IGuideDataProperty : public IUnknown
{
public:
    virtual  HRESULT STDMETHODCALLTYPE get_Name( BSTR *pbstrName )=0;
    virtual  HRESULT STDMETHODCALLTYPE get_Language( long *idLang )=0;
    virtual  HRESULT STDMETHODCALLTYPE get_Value( VARIANT *pvar )=0;
};

class IMpeg2Stream : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Initialize( MPEG_REQUEST_TYPE requestType,
        IMpeg2Data *pMpeg2Data, PMPEG_CONTEXT pContext, PID pid, TID tid,
        PMPEG2_FILTER pFilter, HANDLE hDataReadyEvent )=0;
    virtual HRESULT STDMETHODCALLTYPE SupplyDataBuffer(
        PMPEG_STREAM_BUFFER pStreamBuffer )=0;
};

class ISectionList : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Initialize( MPEG_REQUEST_TYPE requestType,
        IMpeg2Data *pMpeg2Data, PMPEG_CONTEXT pContext, PID pid, TID tid,
        PMPEG2_FILTER pFilter, DWORD timeout, HANDLE hDoneEvent )=0;
    virtual HRESULT STDMETHODCALLTYPE InitializeWithRawSections(
        PMPEG_PACKET_LIST pmplSections )=0;
    virtual HRESULT STDMETHODCALLTYPE CancelPendingRequest( void )=0;
    virtual HRESULT STDMETHODCALLTYPE GetNumberOfSections( WORD *pCount )=0;
    virtual HRESULT STDMETHODCALLTYPE GetSectionData( WORD sectionNumber,
        DWORD *pdwRawPacketLength, PSECTION *ppSection )=0;
    virtual HRESULT STDMETHODCALLTYPE GetProgramIdentifier( PID *pPid )=0;
    virtual HRESULT STDMETHODCALLTYPE GetTableIdentifier( TID *pTableId )=0;
};

class IEnumGuideDataProperties : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Next( unsigned long celt,
        IGuideDataProperty **ppprop, unsigned long *pcelt )=0;
    virtual HRESULT STDMETHODCALLTYPE Skip( unsigned long celt )=0;
    virtual HRESULT STDMETHODCALLTYPE Reset( void )=0;
    virtual HRESULT STDMETHODCALLTYPE Clone( IEnumGuideDataProperties **ppenum )=0;
};

class IEnumTuneRequests : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Next( unsigned long celt, ITuneRequest **ppprop,
        unsigned long *pcelt )=0;
    virtual HRESULT STDMETHODCALLTYPE Skip( unsigned long celt )=0;
    virtual HRESULT STDMETHODCALLTYPE Reset( void )=0;
    virtual HRESULT STDMETHODCALLTYPE Clone( IEnumTuneRequests **ppenum )=0;
};

extern "C" {
DEFINE_GUID(CLSID_DigitalCableNetworkType,
    0x143827AB,0xF77B,0x498d,0x81,0xCA,0x5A,0x00,0x7A,0xEC,0x28,0xBF);

/* KSCATEGORY_BDA */
DEFINE_GUID(KSCATEGORY_BDA_NETWORK_PROVIDER,
    0x71985F4B,0x1CA1,0x11d3,0x9C,0xC8,0x00,0xC0,0x4F,0x79,0x71,0xE0);
DEFINE_GUID(KSCATEGORY_BDA_TRANSPORT_INFORMATION,
    0xa2e3074f,0x6c3d,0x11d3,0xb6,0x53,0x00,0xc0,0x4f,0x79,0x49,0x8e);
DEFINE_GUID(KSCATEGORY_BDA_RECEIVER_COMPONENT,
    0xFD0A5AF4,0xB41D,0x11d2,0x9c,0x95,0x00,0xc0,0x4f,0x79,0x71,0xe0);
DEFINE_GUID(KSCATEGORY_BDA_NETWORK_TUNER,
    0x71985f48,0x1ca1,0x11d3,0x9c,0xc8,0x00,0xc0,0x4f,0x79,0x71,0xe0);
DEFINE_GUID(KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT,
    0xF4AEB342,0x0329,0x4fdd,0xA8,0xFD,0x4A,0xFF,0x49,0x26,0xC9,0x78);

extern const CLSID CLSID_SampleGrabber; // found in strmiids

DEFINE_GUID(IID_IMpeg2Data,
    0x9B396D40,0xF380,0x4e3c,0xA5,0x14,0x1A,0x82,0xBF,0x6E,0xBF,0xE6);

};
