/*****************************************************************************
 * buffer.c : DirectMedia Object decoder module for vlc
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_codec.h>

#ifndef _WIN32
#    define LOADER
#else
#   include <objbase.h>
#endif

#ifdef LOADER
#   include <wine/winerror.h>
#   include <wine/windef.h>
#endif

#include "dmo.h"

static HRESULT STDCALL QueryInterface( IMediaBuffer *This,
                                    const GUID *riid, void **ppv )
{
    CMediaBuffer *p_mb = container_of(This, CMediaBuffer, intf);
    if( !memcmp( riid, &IID_IUnknown, sizeof(GUID) ) ||
        !memcmp( riid, &IID_IMediaBuffer, sizeof(GUID) ) )
    {
        p_mb->i_ref++;
        *ppv = (void *)This;
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

static ULONG STDCALL AddRef( IMediaBuffer *This )
{
    CMediaBuffer *p_mb = container_of(This, CMediaBuffer, intf);
    return p_mb->i_ref++;
}

static ULONG STDCALL Release( IMediaBuffer *This )
{
    CMediaBuffer *p_mb = container_of(This, CMediaBuffer, intf);
    p_mb->i_ref--;

    if( p_mb->i_ref == 0 )
    {
        if( p_mb->b_own ) block_Release( p_mb->p_block );
        free( p_mb );
    }

    return 0;
}

static HRESULT STDCALL SetLength( IMediaBuffer *This, DWORD cbLength )
{
    CMediaBuffer *p_mb = container_of(This, CMediaBuffer, intf);
    if( cbLength > (uint32_t)p_mb->i_max_size ) return E_INVALIDARG;
    p_mb->p_block->i_buffer = cbLength;
    return S_OK;
}

static HRESULT STDCALL GetMaxLength( IMediaBuffer *This, DWORD *pcbMaxLength )
{
    CMediaBuffer *p_mb = container_of(This, CMediaBuffer, intf);
    if( !pcbMaxLength ) return E_POINTER;
    *pcbMaxLength = p_mb->i_max_size;
    return S_OK;
}

static HRESULT STDCALL GetBufferAndLength( IMediaBuffer *This,
                                        BYTE **ppBuffer, DWORD *pcbLength )
{
    CMediaBuffer *p_mb = container_of(This, CMediaBuffer, intf);

    if( !ppBuffer && !pcbLength ) return E_POINTER;
    if( ppBuffer ) *ppBuffer = (BYTE*)p_mb->p_block->p_buffer;
    if( pcbLength ) *pcbLength = p_mb->p_block->i_buffer;
    return S_OK;
}

static CONST_VTBL IMediaBufferVtbl CMediaBuffer_vtable = {
    QueryInterface, AddRef, Release,
    SetLength, GetMaxLength, GetBufferAndLength,
};

CMediaBuffer *CMediaBufferCreate( block_t *p_block, int i_max_size,
                                  bool b_own )
{
    CMediaBuffer *p_mb = (CMediaBuffer *)malloc( sizeof(CMediaBuffer) );
    if( !p_mb ) return NULL;
    p_mb->intf.lpVtbl = &CMediaBuffer_vtable;

    p_mb->i_ref = 1;
    p_mb->p_block = p_block;
    p_mb->i_max_size = i_max_size;
    p_mb->b_own = b_own;

    return p_mb;
}
