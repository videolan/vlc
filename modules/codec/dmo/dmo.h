/*****************************************************************************
 * dmo.h : DirectMedia Object codec module for vlc
 *****************************************************************************
 * Copyright (C) 2002, 2003 VLC authors and VideoLAN
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

extern const GUID IID_IUnknown;
extern const GUID IID_IClassFactory;
extern const GUID IID_IWMCodecPrivateData;
extern const GUID IID_IMediaObject;
extern const GUID IID_IMediaBuffer;
extern const GUID MEDIATYPE_Video;
extern const GUID MEDIATYPE_Audio;
extern const GUID MEDIASUBTYPE_PCM;
extern const GUID FORMAT_VideoInfo;
extern const GUID FORMAT_WaveFormatEx;
extern const GUID GUID_NULL;
extern const GUID MEDIASUBTYPE_I420;
extern const GUID MEDIASUBTYPE_YV12;
extern const GUID MEDIASUBTYPE_RGB24;
extern const GUID MEDIASUBTYPE_RGB565;


#ifndef _WIN32
void* CoTaskMemAlloc(unsigned long cb);
void CoTaskMemFree(void* cb);
#endif

#define IUnknown IUnknownHack
#define IClassFactory IClassFactoryHack
typedef struct _IUnknown IUnknown;
typedef struct _IClassFactory IClassFactory;
typedef struct _IWMCodecPrivateData IWMCodecPrivateData;
typedef struct _IEnumDMO IEnumDMO;
typedef struct _IMediaBuffer IMediaBuffer;
typedef struct _IMediaObject IMediaObject;

#ifndef STDCALL
#define STDCALL __stdcall
#endif

#define DMO_INPUT_DATA_BUFFERF_SYNCPOINT 1
#define DMO_INPUT_DATA_BUFFERF_TIME 2
#define DMO_INPUT_DATA_BUFFERF_TIMELENGTH 4
#define DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT 1
#define DMO_OUTPUT_DATA_BUFFERF_TIME 2
#define DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH 4
#define DMO_PROCESS_OUTPUT_DISCARD_WHEN_NO_BUFFER 1
#define DMO_E_NOTACCEPTING 0x80040204

/*
 * DMO types definition
 */
typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
 _DMO_PARTIAL_MEDIATYPE
{
    GUID type;
    GUID subtype;

} DMO_PARTIAL_MEDIATYPE;

typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
 _DMO_OUTPUT_DATA_BUFFER
{
    IMediaBuffer *pBuffer;
    uint32_t dwStatus;
    REFERENCE_TIME rtTimestamp;
    REFERENCE_TIME rtTimelength;

} DMO_OUTPUT_DATA_BUFFER;

typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
 _DMOMediaType
{
    GUID     majortype;
    GUID     subtype;
    int      bFixedSizeSamples;
    int      bTemporalCompression;
    uint32_t lSampleSize;
    GUID     formattype;
    IUnknown *pUnk;
    uint32_t cbFormat;
    char     *pbFormat;

} DMO_MEDIA_TYPE;

/*
 * IUnknown interface
 */
typedef struct IUnknown_vt
{
    /* IUnknown methods */
    long (STDCALL *QueryInterface)(IUnknown *This, const GUID *riid,
                                   void **ppvObject);
    long (STDCALL *AddRef)(IUnknown *This);
    long (STDCALL *Release)(IUnknown *This);

} IUnknown_vt;
struct _IUnknown { IUnknown_vt* vt; };

/*
 * IClassFactory interface
 */
typedef struct IClassFactory_vt
{
    long (STDCALL *QueryInterface)(IUnknown *This, const GUID* riid,
                                   void **ppvObject);
    long (STDCALL *AddRef)(IUnknown *This) ;
    long (STDCALL *Release)(IUnknown *This) ;
    long (STDCALL *CreateInstance)(IClassFactory *This, IUnknown *pUnkOuter,
                                   const GUID* riid, void** ppvObject);
} IClassFactory_vt;

struct _IClassFactory { IClassFactory_vt* vt; };

/*
 * IWMCodecPrivateData interface
 */
typedef struct IWMCodecPrivateData_vt
{
    long (STDCALL *QueryInterface)(IUnknown *This, const GUID* riid,
                                   void **ppvObject);
    long (STDCALL *AddRef)(IUnknown *This) ;
    long (STDCALL *Release)(IUnknown *This) ;

 
    long (STDCALL *SetPartialOutputType)(IWMCodecPrivateData * This,
                                         DMO_MEDIA_TYPE *pmt);

    long (STDCALL *GetPrivateData )(IWMCodecPrivateData * This,
                                    uint8_t *pbData, uint32_t *pcbData);
} IWMCodecPrivateData_vt;

struct _IWMCodecPrivateData { IWMCodecPrivateData_vt* vt; };

/*
 * IEnumDMO interface
 */
typedef struct IEnumDMO_vt
{
    /* IUnknown methods */
    long (STDCALL *QueryInterface)(IUnknown *This, const GUID *riid,
                                   void **ppvObject);
    long (STDCALL *AddRef)(IUnknown *This);
    long (STDCALL *Release)(IUnknown *This);

    /* IEnumDMO methods */
    long (STDCALL *Next)(IEnumDMO *This, uint32_t cItemsToFetch,
                         const GUID *pCLSID, WCHAR **Names,
                         uint32_t *pcItemsFetched);
    long (STDCALL *Skip)(IEnumDMO *This, uint32_t cItemsToSkip);
    long (STDCALL *Reset)(IEnumDMO *This);
    long (STDCALL *Clone)(IEnumDMO *This, IEnumDMO **ppEnum);

} IEnumDMO_vt;
struct _IEnumDMO { IEnumDMO_vt* vt; };

/*
 * IMediaBuffer interface
 */
typedef struct IMediaBuffer_vt
{
    /* IUnknown methods */
    long (STDCALL *QueryInterface)(IUnknown *This, const GUID *riid,
                                   void **ppvObject);
    long (STDCALL *AddRef)(IUnknown *This);
    long (STDCALL *Release)(IUnknown *This);

    long (STDCALL *SetLength)(IMediaBuffer* This, uint32_t cbLength);
    long (STDCALL *GetMaxLength)(IMediaBuffer* This, uint32_t *pcbMaxLength);
    long (STDCALL *GetBufferAndLength)(IMediaBuffer* This,
                                       char **ppBuffer, uint32_t *pcbLength);

} IMediaBuffer_vt;
struct _IMediaBuffer { IMediaBuffer_vt* vt; };

/*
 * IMediaObject interface
 */
typedef struct IMediaObject_vt
{
    /* IUnknown methods */
    long (STDCALL *QueryInterface)(IUnknown *This, const GUID *riid,
                                   void **ppvObject);
    long (STDCALL *AddRef)(IUnknown *This);
    long (STDCALL *Release)(IUnknown *This);

    /* IEnumDMO methods */
    long (STDCALL *GetStreamCount)(IMediaObject *This,
                                   uint32_t *pcInputStreams,
                                   uint32_t *pcOutputStreams);
    long (STDCALL *GetInputStreamInfo)(IMediaObject *This,
                                       uint32_t dwInputStreamIndex,
                                       uint32_t *pdwFlags);
    long (STDCALL *GetOutputStreamInfo)(IMediaObject *This,
                                        uint32_t dwOutputStreamIndex,
                                        uint32_t *pdwFlags);
    long (STDCALL *GetInputType)(IMediaObject *This,
                                 uint32_t dwInputStreamIndex,
                                 uint32_t dwTypeIndex,
                                 DMO_MEDIA_TYPE *pmt);
    long (STDCALL *GetOutputType)(IMediaObject *This,
                                  uint32_t dwOutputStreamIndex,
                                  uint32_t dwTypeIndex,
                                  DMO_MEDIA_TYPE *pmt);
    long (STDCALL *SetInputType)(IMediaObject *This,
                                 uint32_t dwInputStreamIndex,
                                 const DMO_MEDIA_TYPE *pmt,
                                 uint32_t dwFlags);
    long (STDCALL *SetOutputType)(IMediaObject *This,
                                  uint32_t dwOutputStreamIndex,
                                  const DMO_MEDIA_TYPE *pmt,
                                  uint32_t dwFlags);
    long (STDCALL *GetInputCurrentType)(IMediaObject *This,
                                        uint32_t dwInputStreamIndex,
                                        DMO_MEDIA_TYPE *pmt);
    long (STDCALL *GetOutputCurrentType)(IMediaObject *This,
                                         uint32_t dwOutputStreamIndex,
                                         DMO_MEDIA_TYPE *pmt);
    long (STDCALL *GetInputSizeInfo)(IMediaObject *This,
                                     uint32_t dwInputStreamIndex,
                                     uint32_t *pcbSize,
                                     uint32_t *pcbMaxLookahead,
                                     uint32_t *pcbAlignment);
    long (STDCALL *GetOutputSizeInfo)(IMediaObject *This,
                                      uint32_t dwOutputStreamIndex,
                                      uint32_t *pcbSize,
                                      uint32_t *pcbAlignment);
    long (STDCALL *GetInputMaxLatency)(IMediaObject *This,
                                       uint32_t dwInputStreamIndex,
                                       REFERENCE_TIME *prtMaxLatency);
    long (STDCALL *SetInputMaxLatency)(IMediaObject *This,
                                       uint32_t dwInputStreamIndex,
                                       REFERENCE_TIME rtMaxLatency);
    long (STDCALL *Flush)(IMediaObject * This);
    long (STDCALL *Discontinuity)(IMediaObject *This,
                                  uint32_t dwInputStreamIndex);
    long (STDCALL *AllocateStreamingResources)(IMediaObject * This);
    long (STDCALL *FreeStreamingResources)(IMediaObject * This);
    long (STDCALL *GetInputStatus)(IMediaObject *This,
                                   uint32_t dwInputStreamIndex,
                                   uint32_t *dwFlags);
    long (STDCALL *ProcessInput)(IMediaObject *This,
                                 uint32_t dwInputStreamIndex,
                                 IMediaBuffer *pBuffer,
                                 uint32_t dwFlags,
                                 REFERENCE_TIME rtTimestamp,
                                 REFERENCE_TIME rtTimelength);
    long (STDCALL *ProcessOutput)(IMediaObject *This,
                                  uint32_t dwFlags,
                                  uint32_t cOutputBufferCount,
                                  DMO_OUTPUT_DATA_BUFFER *pOutputBuffers,
                                  uint32_t *pdwStatus);
    long (STDCALL *Lock)(IMediaObject *This, long bLock);

} IMediaObject_vt;
struct _IMediaObject { IMediaObject_vt* vt; };

/* Implementation of IMediaBuffer */
typedef struct _CMediaBuffer
{
    IMediaBuffer_vt *vt;
    int i_ref;
    block_t *p_block;
    int i_max_size;
    bool b_own;

} CMediaBuffer;

CMediaBuffer *CMediaBufferCreate( block_t *, int, bool );
