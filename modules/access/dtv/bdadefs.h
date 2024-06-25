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
#include <mpeg2structs.h>

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

};
